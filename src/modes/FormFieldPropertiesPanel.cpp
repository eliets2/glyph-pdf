// SPDX-License-Identifier: Apache-2.0
#include "FormFieldPropertiesPanel.h"
#include "commands/EditFormFieldCommand.h"
#include "core/AppContext.h"
#include "core/FormStaleFieldTracker.h"
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"
#include "shell/EditPolicy.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QDoubleSpinBox>

namespace gp {

FormFieldPropertiesPanel::FormFieldPropertiesPanel(const AppContext* ctx, QWidget* parent)
    : QWidget(parent)
    , m_ctx(ctx)
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(4, 4, 4, 4);
    col->setSpacing(6);

    auto* title = new QLabel(tr("Field Properties"));
    title->setProperty("mono", true);
    col->addWidget(title);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("field_name"));
    connect(m_nameEdit, &QLineEdit::textChanged, this, &FormFieldPropertiesPanel::onNameChanged);
    form->addRow(tr("Name:"), m_nameEdit);

    m_nameStatus = new QLabel;
    m_nameStatus->setStyleSheet("QLabel { color: #c00; font-size: 10px; }");
    // PGR-35: status labels surface DOCUMENT-derived text (field names, form-JS
    // failure reasons, format-script output). Plain-text format means a hostile
    // payload can never be auto-detected and rendered as rich-text markup.
    m_nameStatus->setTextFormat(Qt::PlainText);
    m_nameStatus->setVisible(false);
    form->addRow(QString(), m_nameStatus);

    m_tooltipEdit = new QLineEdit;
    m_tooltipEdit->setPlaceholderText(tr("Tooltip text"));
    form->addRow(tr("Tooltip:"), m_tooltipEdit);

    m_requiredCheck = new QCheckBox(tr("Required"));
    form->addRow(QString(), m_requiredCheck);

    m_defaultEdit = new QLineEdit;
    m_defaultEdit->setPlaceholderText(tr("Default value"));
    m_defaultEdit->setObjectName(QStringLiteral("defaultValueEdit"));
    // R18(f): every text-changing edit runs the field's /AA /K Keystroke
    // script (Acrobat semantics) — the edit is reverted when the script
    // rejects it, and the field shows the script-transformed text when it
    // rewrites event.value (AFMergeChange idiom). The sandbox caps apply:
    // one 250 ms whole-operation budget per keystroke, zero I/O, egress
    // verbs hard no-ops (FormJsSandbox).
    connect(m_defaultEdit, &QLineEdit::textChanged, this, &FormFieldPropertiesPanel::onDefaultTextChanged);
    form->addRow(tr("Default:"), m_defaultEdit);

    // R18(f): the honest disclosure for what the keystroke gate did — a
    // rejected or script-transformed keystroke is never silent.
    m_keystrokeStatus = new QLabel;
    m_keystrokeStatus->setObjectName(QStringLiteral("keystrokeStatus"));
    m_keystrokeStatus->setStyleSheet("QLabel { color: #b00; font-size: 10px; }");
    m_keystrokeStatus->setWordWrap(true);
    m_keystrokeStatus->setTextFormat(Qt::PlainText); // PGR-35 (see above)
    m_keystrokeStatus->setVisible(false);
    form->addRow(QString(), m_keystrokeStatus);

    // Phase-1 form-JS (U08 idiom): a calculated field is NAMED before the user
    // wonders why its value changes, and a format script's effect is shown as
    // a clearly-labeled display preview (presentation only, /V is untouched).
    m_scriptBadge = new QLabel;
    m_scriptBadge->setStyleSheet("QLabel { color: #06c; font-size: 10px; }");
    m_scriptBadge->setWordWrap(true);
    m_scriptBadge->setTextFormat(Qt::PlainText); // PGR-35 (see above)
    m_scriptBadge->setVisible(false);
    form->addRow(QString(), m_scriptBadge);

    m_displayPreview = new QLabel;
    m_displayPreview->setObjectName(QStringLiteral("displayPreview"));
    m_displayPreview->setStyleSheet("QLabel { color: #666; font-size: 10px; }");
    m_displayPreview->setWordWrap(true);
    // PGR-35 (the sharpest site): this label shows a FORMAT SCRIPT'S OUTPUT —
    // fully document-controlled text. Plain-text format is mandatory.
    m_displayPreview->setTextFormat(Qt::PlainText);
    m_displayPreview->setVisible(false);
    form->addRow(QString(), m_displayPreview);

    // R18(a): the PERSISTENT stale-calculated-field warning. A cascade failure
    // leaves the field's committed value in place — the disclosure survives
    // here (backed by AppContext's FormStaleFieldTracker) until the field
    // recomputes or the user acknowledges it; it is not a transient dialog.
    m_staleBanner = new QLabel;
    m_staleBanner->setStyleSheet("QLabel { color: #b00; font-size: 10px; }");
    m_staleBanner->setWordWrap(true);
    m_staleBanner->setTextFormat(Qt::PlainText); // PGR-35 (see above)
    m_staleBanner->setVisible(false);
    form->addRow(QString(), m_staleBanner);
    auto* staleRow = new QHBoxLayout;
    m_staleAckBtn = new QToolButton;
    m_staleAckBtn->setText(tr("Acknowledge — keep the stored value"));
    m_staleAckBtn->setToolTip(tr("Dismiss the stale-value warning. The field's stored value is "
                                 "unchanged; it will keep not recomputing until its script runs "
                                 "successfully."));
    m_staleAckBtn->setVisible(false);
    staleRow->addStretch(1);
    staleRow->addWidget(m_staleAckBtn);
    connect(m_staleAckBtn, &QToolButton::clicked, this, [this] {
        if (m_ctx && m_ctx->formStale && m_ctx->document && !m_fieldName.isEmpty())
            m_ctx->formStale->acknowledge(m_ctx->document->path(), m_fieldName);
        refreshScriptState();
    });
    form->addRow(QString(), staleRow);

    m_placeholderEdit = new QLineEdit;
    m_placeholderEdit->setPlaceholderText(tr("Placeholder text"));
    form->addRow(tr("Placeholder:"), m_placeholderEdit);

    m_regexEdit = new QLineEdit;
    m_regexEdit->setPlaceholderText(tr("Validation regex (optional)"));
    connect(m_regexEdit, &QLineEdit::textChanged, this, &FormFieldPropertiesPanel::onRegexChanged);
    form->addRow(tr("Regex:"), m_regexEdit);

    m_regexStatus = new QLabel;
    m_regexStatus->setStyleSheet("QLabel { color: #c00; font-size: 10px; }");
    m_regexStatus->setTextFormat(Qt::PlainText); // PGR-35 (see above)
    m_regexStatus->setVisible(false);
    form->addRow(QString(), m_regexStatus);

    auto addSpin = [this](QDoubleSpinBox*& spin) {
        spin = new QDoubleSpinBox;
        spin->setRange(0, 9999);
        spin->setDecimals(1);
    };
    addSpin(m_spinX);
    addSpin(m_spinY);
    addSpin(m_spinW);
    addSpin(m_spinH);
    
    auto* geomLayout = new QHBoxLayout;
    geomLayout->addWidget(new QLabel(tr("X:"))); geomLayout->addWidget(m_spinX);
    geomLayout->addWidget(new QLabel(tr("Y:"))); geomLayout->addWidget(m_spinY);
    geomLayout->addWidget(new QLabel(tr("W:"))); geomLayout->addWidget(m_spinW);
    geomLayout->addWidget(new QLabel(tr("H:"))); geomLayout->addWidget(m_spinH);
    form->addRow(tr("Geometry:"), geomLayout);

    col->addLayout(form);
    col->addStretch(1);

    m_applyBtn = new QToolButton;
    m_applyBtn->setText(tr("Apply"));
    m_applyBtn->setObjectName(QStringLiteral("applyFieldPropsButton"));
    m_applyBtn->setProperty("variant", "primary");
    m_applyBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_applyBtn, &QToolButton::clicked, this, &FormFieldPropertiesPanel::onApplyClicked);
    col->addWidget(m_applyBtn);

    // emergence E-1 (SWEEP-W3-EMERGENCE §6): the Apply path persists through
    // EditFormFieldCommand → applyFieldSnapshot → runFormSaveTransaction — a
    // direct mutation entry that must follow the ONE read-only policy (ARC07).
    // An expired document's form properties are not writable: the button is
    // disabled for the session's read-only lifetime (and re-enabled if the
    // session ever becomes writable again). The slot keeps its own gate as
    // defense in depth — a disabled button alone is affordance, not policy.
    if (m_ctx && m_ctx->document) {
        connect(m_ctx->document.get(), &DocumentSession::readOnlyChanged,
                this, [this](bool readOnly) {
                    if (m_applyBtn) m_applyBtn->setEnabled(!readOnly);
                });
        m_applyBtn->setEnabled(!m_ctx->document->isReadOnly());
    }
}

void FormFieldPropertiesPanel::setFieldName(const QString& name)
{
    m_fieldName = name;
    m_nameEdit->setText(name);
    m_tooltipEdit->clear();
    m_requiredCheck->setChecked(false);
    setValueText(QString());
    m_placeholderEdit->clear();
    m_regexEdit->clear();
    m_nameStatus->setVisible(false);
    m_regexStatus->setVisible(false);
    if (m_keystrokeStatus) m_keystrokeStatus->setVisible(false);
    refreshScriptState();
}

void FormFieldPropertiesPanel::clearFields()
{
    m_fieldName.clear();
    m_nameEdit->clear();
    m_tooltipEdit->clear();
    m_requiredCheck->setChecked(false);
    setValueText(QString());
    m_placeholderEdit->clear();
    m_regexEdit->clear();
    m_nameStatus->setVisible(false);
    m_regexStatus->setVisible(false);
    m_scriptBadge->setVisible(false);
    m_displayPreview->setVisible(false);
    if (m_keystrokeStatus) m_keystrokeStatus->setVisible(false);
    if (m_staleBanner) m_staleBanner->setVisible(false);
    if (m_staleAckBtn) m_staleAckBtn->setVisible(false);
}

void FormFieldPropertiesPanel::setValueText(const QString& text)
{
    // Programmatic writes (population, keystroke revert, script transform)
    // never run the keystroke event — only the USER's edit does.
    m_syncingValueText = true;
    m_defaultEdit->setText(text);
    m_syncingValueText = false;
    m_keystrokeBase = text;
}

void FormFieldPropertiesPanel::onDefaultTextChanged(const QString& text)
{
    if (!m_keystrokeStatus || !m_defaultEdit) return;
    if (m_syncingValueText) { m_keystrokeBase = text; return; } // belt and braces
    m_keystrokeStatus->setVisible(false);
    if (text == m_keystrokeBase) return;

    // emergence E-1 (SWEEP-W3-EMERGENCE §6): on a read-only (expired)
    // document the panel runs no /AA /K script at all — the keystroke event
    // is an engine execution the session's ONE read-only policy refuses.
    // Typing stands as plain text and the refusal is disclosed.
    if (EditPolicy::mutationBlocked(m_ctx && m_ctx->document ? m_ctx->document.get() : nullptr)) {
        m_keystrokeBase = text;
        m_keystrokeStatus->setStyleSheet("QLabel { color: #b00; font-size: 10px; }");
        m_keystrokeStatus->setText(EditPolicy::readOnlyMessage());
        m_keystrokeStatus->setVisible(true);
        return;
    }

    const QString before = m_keystrokeBase;
    const QString path = (m_ctx && m_ctx->document) ? m_ctx->document->path() : QString();
    if (m_fieldName.isEmpty() || path.isEmpty() || !m_ctx || !m_ctx->forms) {
        m_keystrokeBase = text; // no form context: plain typing, no event
        return;
    }

    // The edit in Acrobat's event shape: [selStart, selEnd) of the OLD text
    // was replaced by `change`. Common-prefix/suffix trim reduces end
    // insertions, end deletions, mid-string edits and select-all+type to
    // this one model — exactly what the shim's AFMergeChange splices.
    int p = 0;
    const int maxP = qMin(before.size(), text.size());
    while (p < maxP && before.at(p) == text.at(p)) ++p;
    int s = 0;
    const int maxS = qMin(before.size(), text.size()) - p;
    while (s < maxS && before.at(before.size() - 1 - s) == text.at(text.size() - 1 - s)) ++s;
    const QString change = text.mid(p, text.size() - p - s);

    FormJsFailure failure;
    const auto r = m_ctx->forms->runKeystrokeEvent(path, m_fieldName, before, change,
                                                   p, before.size() - s, &failure);
    if (!r.ran) {
        m_keystrokeBase = text; // no runnable /AA /K: typing stands
        // An engine-level failure (e.g. the document could not be loaded)
        // must not silently pretend the gate approved — disclose it.
        if (!failure.kind.isEmpty()) {
            m_keystrokeStatus->setStyleSheet("QLabel { color: #b00; font-size: 10px; }");
            m_keystrokeStatus->setText(tr("Keystroke script not evaluated (%1): %2 — "
                                          "the text was kept.").arg(failure.kind, failure.reason));
            m_keystrokeStatus->setVisible(true);
        }
        return;
    }

    if (!r.allowed) {
        // Acrobat semantics: rc=false (or any script failure) REJECTS the
        // keystroke — the edit never took; the line edit reverts.
        if (text != before) setValueText(before);
        m_keystrokeStatus->setStyleSheet("QLabel { color: #b00; font-size: 10px; }");
        m_keystrokeStatus->setText(tr("Keystroke blocked (%1): %2").arg(failure.kind, failure.reason));
        m_keystrokeStatus->setVisible(true);
        return;
    }
    if (!r.valueToApply.isEmpty() && r.valueToApply != text) {
        // The script TRANSFORMED the proposal (event.value) — the field shows
        // the transformed text (Acrobat's filtered-keystroke idiom).
        setValueText(r.valueToApply);
        m_keystrokeStatus->setText(tr("Adjusted by the field's keystroke script."));
        m_keystrokeStatus->setStyleSheet("QLabel { color: #666; font-size: 10px; }");
        m_keystrokeStatus->setVisible(true);
        return;
    }
    m_keystrokeBase = text;
}

void FormFieldPropertiesPanel::refreshScriptState()
{
    if (!m_scriptBadge || !m_displayPreview) return;
    const QString path = (m_ctx && m_ctx->document) ? m_ctx->document->path() : QString();
    if (m_fieldName.isEmpty() || path.isEmpty() || !m_ctx || !m_ctx->forms) {
        m_scriptBadge->setVisible(false);
        m_displayPreview->setVisible(false);
        if (m_staleBanner) m_staleBanner->setVisible(false);
        if (m_staleAckBtn) m_staleAckBtn->setVisible(false);
        return;
    }

    // R18(a): the stale warning is derived from the session tracker on EVERY
    // refresh — it survives panel rebuilds and document switches and clears
    // only when the field recomputes (the tracker's replace rule) or the user
    // acknowledges it.
    if (m_ctx->formStale && m_staleBanner && m_staleAckBtn) {
        const bool stale = m_ctx->formStale->isStale(path, m_fieldName);
        if (stale) {
            m_staleBanner->setText(tr("STALE VALUE WARNING: the last calculation run did not "
                                      "update this field (%1). The value shown is the field's "
                                      "stored value and may be out of date relative to the "
                                      "fields it is calculated from.").arg(
                                       m_ctx->formStale->staleReason(path, m_fieldName)));
        }
        m_staleBanner->setVisible(stale);
        m_staleAckBtn->setVisible(stale);
    }

    const bool calculated = m_ctx->forms->fieldHasCalculateScript(path, m_fieldName);
    if (calculated) {
        m_scriptBadge->setText(tr("Calculated field — the value is recomputed from the "
                                  "document's calculation order when values are committed "
                                  "or saved. Typing here will not stick."));
    } else {
        m_scriptBadge->setVisible(false);
    }

    const bool formatted = m_ctx->forms->fieldHasFormatScript(path, m_fieldName);
    if (formatted) {
        FormJsFailure failure;
        const QString display = m_ctx->forms->formatFieldValue(path, m_fieldName, &failure);
        if (!failure.kind.isEmpty()) {
            // Honest: a failed format script is reported, not silently hidden.
            m_displayPreview->setText(tr("Format script failed (%1): %2 — the stored value is "
                                         "shown unchanged.").arg(failure.kind, failure.reason));
        } else {
            m_displayPreview->setText(tr("Display preview (format script — presentation only, "
                                         "the stored value is unchanged): %1").arg(display));
        }
        m_displayPreview->setVisible(true);
    } else {
        m_displayPreview->setVisible(false);
    }
    if (calculated) m_scriptBadge->setVisible(true);
}

void FormFieldPropertiesPanel::onApplyClicked()
{
    // emergence E-1 (SWEEP-W3-EMERGENCE §6): the Apply entry is a DIRECT
    // mutation entry (EditFormFieldCommand → applyFieldSnapshot →
    // runFormSaveTransaction → atomic commit) that bypassed the ONE
    // read-only policy — on an expired document it persisted edits and ran
    // the calculate cascade. ARC07 gate first, honest refusal, zero mutation.
    if (EditPolicy::mutationBlocked(m_ctx ? m_ctx->document.get() : nullptr)) {
        if (m_keystrokeStatus) {
            m_keystrokeStatus->setStyleSheet("QLabel { color: #b00; font-size: 10px; }");
            m_keystrokeStatus->setText(EditPolicy::readOnlyMessage());
            m_keystrokeStatus->setVisible(true);
        }
        return;
    }

    // Validate before pushing command
    validateName();
    validateRegex();

    if (m_nameStatus->isVisible() || m_regexStatus->isVisible()) return;
    if (m_fieldName.isEmpty()) return;
    if (!m_ctx || !m_ctx->undoStack || !m_ctx->forms || !m_ctx->document) return;

    EditFormFieldProperties newProps;
    newProps.name        = m_nameEdit->text().trimmed();
    newProps.tooltip     = m_tooltipEdit->text();
    newProps.required    = m_requiredCheck->isChecked();
    newProps.defaultVal  = m_defaultEdit->text();
    newProps.placeholder = m_placeholderEdit->text();
    newProps.validRegex  = m_regexEdit->text();

    QList<FormJsFailure> jsFailures;
    // r18-review F1: the apply result is the COMMAND's explicit outcome,
    // captured into this caller-owned sink during push's initial redo (the
    // sink outlives a push that deletes an obsolete command). The old
    // stack-count heuristic could not tell an applied command from an
    // undo-limit discard (count() stays EQUAL — the oldest entry is deleted)
    // or a redo-stack flush (count() DROPS, e.g. edit after undo) and skipped
    // the stale-field feed exactly when a real recompute happened.
    bool applyResult = false;
    auto* cmd = new EditFormFieldCommand(
        m_ctx->forms.get(),
        m_ctx->document.get(),
        m_fieldName,
        newProps,
        &jsFailures,
        m_ctx->formStale.get(),   // r18-review F2: the command feeds the tracker
        &applyResult
    );
    m_ctx->undoStack->push(cmd);

    const QString applied = newProps.name.isEmpty() ? m_fieldName : newProps.name;
    m_fieldName = applied;
    emit propertiesApplied(applied);
    emit geometryCommitted(fieldRect());

    // r18-review F1/F2: the stale-field tracker is fed by the COMMAND at the
    // one shared committed-transaction boundary (panel apply, redo traversal
    // AND undo/redo restore) — the panel no longer derives success from stack
    // arithmetic, and history traversal updates the stale state too.

    // Phase-1 form-JS honesty contract: the edit persisted, but calculated
    // fields whose scripts failed are named — never a silent wrong value.
    // The persistent banner (refreshScriptState) carries the same disclosure
    // until the field recomputes or the user acknowledges it.
    if (!jsFailures.isEmpty()) {
        QStringList lines;
        for (const FormJsFailure& f : jsFailures)
            lines << tr("• %1 — %2 (%3)").arg(f.fieldName, f.reason, f.kind);
        // PGR-35: the lines carry document-derived text (field names + the
        // scripts' own failure messages) — plain text, never rich-text.
        QMessageBox box(QMessageBox::Warning, tr("Field saved, calculation failed"),
            tr("The field was saved, but %n calculated field(s) failed and kept "
               "their previous value:\n\n%1", "", jsFailures.size()).arg(lines.join('\n')),
            QMessageBox::Ok, this);
        box.setTextFormat(Qt::PlainText);
        box.exec();
    }
    refreshScriptState();
}

void FormFieldPropertiesPanel::setFieldRect(const QRectF& rect)
{
    if (m_spinX) m_spinX->setValue(rect.x());
    if (m_spinY) m_spinY->setValue(rect.y());
    if (m_spinW) m_spinW->setValue(rect.width());
    if (m_spinH) m_spinH->setValue(rect.height());
}

QRectF FormFieldPropertiesPanel::fieldRect() const
{
    if (!m_spinX || !m_spinY || !m_spinW || !m_spinH) return QRectF();
    return QRectF(m_spinX->value(), m_spinY->value(), m_spinW->value(), m_spinH->value());
}

void FormFieldPropertiesPanel::onNameChanged(const QString& /*text*/)
{
    validateName();
}

void FormFieldPropertiesPanel::onRegexChanged(const QString& /*text*/)
{
    validateRegex();
}

void FormFieldPropertiesPanel::validateName()
{
    if (!m_nameStatus) return;
    const QString n = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
    if (n.isEmpty()) {
        m_nameStatus->setText(tr("Name cannot be empty"));
        m_nameStatus->setVisible(true);
        if (m_nameEdit)
            m_nameEdit->setStyleSheet("QLineEdit { border: 1px solid #c00; }");
    } else {
        m_nameStatus->setVisible(false);
        if (m_nameEdit)
            m_nameEdit->setStyleSheet(QString());
    }
}

void FormFieldPropertiesPanel::validateRegex()
{
    if (!m_regexStatus || !m_regexEdit) return;
    const QString pattern = m_regexEdit->text();
    if (pattern.isEmpty()) {
        m_regexStatus->setVisible(false);
        m_regexEdit->setStyleSheet(QString());
        return;
    }
    const QRegularExpression re(pattern);
    if (!re.isValid()) {
        m_regexStatus->setText(tr("Invalid regex: %1").arg(re.errorString()));
        m_regexStatus->setVisible(true);
        m_regexEdit->setStyleSheet("QLineEdit { border: 1px solid #c00; }");
    } else {
        m_regexStatus->setVisible(false);
        m_regexEdit->setStyleSheet(QString());
    }
}

} // namespace gp
