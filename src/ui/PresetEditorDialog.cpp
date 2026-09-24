// SPDX-License-Identifier: Apache-2.0
#include "ui/PresetEditorDialog.h"

#include "core/BatchPreset.h"
#include "core/Capability.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace gp {

using BatchPresetSchema::knownOps;

PresetEditorDialog::PresetEditorDialog(const CapabilityRegistry* capabilities,
                                       QWidget* parent)
    : QDialog(parent), capabilities_(capabilities) {
    setWindowTitle(tr("Preset Editor"));
    setModal(true);
    resize(560, 520);

    auto* lay = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    nameEdit_ = new QLineEdit;
    nameEdit_->setObjectName(QStringLiteral("presetEditorName"));
    nameEdit_->setMaxLength(80);
    nameEdit_->setPlaceholderText(tr("Preset name (1-80 characters)"));
    descriptionEdit_ = new QLineEdit;
    descriptionEdit_->setObjectName(QStringLiteral("presetEditorDescription"));
    descriptionEdit_->setMaxLength(300);
    descriptionEdit_->setPlaceholderText(tr("Optional description"));
    form->addRow(tr("Name:"), nameEdit_);
    form->addRow(tr("Description:"), descriptionEdit_);
    lay->addLayout(form);

    // ── Step list + add palette ──────────────────────────────────────────────
    auto* stepsLay = new QHBoxLayout;
    stepsList_ = new QListWidget;
    stepsList_->setObjectName(QStringLiteral("presetEditorSteps"));
    stepsLay->addWidget(stepsList_, 2);

    auto* stepsBtns = new QVBoxLayout;
    // Op palette: UnavailableBuild ops are not offered (the capability gate —
    // an op structurally absent from this binary cannot be honestly added).
    addOpCombo_ = new QComboBox;
    addOpCombo_->setObjectName(QStringLiteral("presetEditorAddOp"));
    for (const QString& op : knownOps()) {
        const Capability cap =
            batchPresetStepCapability({ op, {}, {} }, capabilities_);
        if (cap.status == Availability::UnavailableBuild)
            continue;
        addOpCombo_->addItem(op);
    }
    addBtn_ = new QPushButton(tr("Add step"));
    addBtn_->setObjectName(QStringLiteral("presetEditorAddBtn"));
    removeBtn_ = new QPushButton(tr("Remove"));
    removeBtn_->setObjectName(QStringLiteral("presetEditorRemoveBtn"));
    upBtn_ = new QPushButton(tr("Up"));
    upBtn_->setObjectName(QStringLiteral("presetEditorUpBtn"));
    downBtn_ = new QPushButton(tr("Down"));
    downBtn_->setObjectName(QStringLiteral("presetEditorDownBtn"));
    stepsBtns->addWidget(addOpCombo_);
    stepsBtns->addWidget(addBtn_);
    stepsBtns->addWidget(removeBtn_);
    stepsBtns->addWidget(upBtn_);
    stepsBtns->addWidget(downBtn_);
    stepsBtns->addStretch(1);
    stepsLay->addLayout(stepsBtns, 1);
    lay->addLayout(stepsLay, 2);

    // ── Per-step parameter form (rebuilt for the selected step) ──────────────
    paramNote_ = new QLabel(tr("Select or add a step to edit its parameters."));
    paramNote_->setWordWrap(true);
    paramNote_->setStyleSheet("color:#71747a; font-size:10px;");
    lay->addWidget(paramNote_);
    paramScroll_ = new QScrollArea;
    paramScroll_->setWidgetResizable(true);
    paramScroll_->setFrameShape(QFrame::NoFrame);
    paramScroll_->setMinimumHeight(140);
    lay->addWidget(paramScroll_, 3);

    errorLabel_ = new QLabel;
    errorLabel_->setObjectName(QStringLiteral("presetEditorError"));
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet("color:#c8442b; font-size:10px;");
    errorLabel_->hide();
    lay->addWidget(errorLabel_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save
                                         | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setObjectName(
        QStringLiteral("presetEditorSaveBtn"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { savePreset(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(buttons);

    connect(addBtn_, &QPushButton::clicked, this, [this] {
        if (addOpCombo_->currentIndex() >= 0)
            addStep(addOpCombo_->currentText());
    });
    connect(removeBtn_, &QPushButton::clicked, this,
            [this] { removeCurrentStep(); });
    connect(upBtn_, &QPushButton::clicked, this, [this] { moveCurrentStep(-1); });
    connect(downBtn_, &QPushButton::clicked, this, [this] { moveCurrentStep(1); });
    connect(stepsList_, &QListWidget::currentRowChanged, this,
            [this](int) { rebuildParamForm(); });
}

void PresetEditorDialog::loadPreset(const BatchPreset& preset) {
    m_preset = preset;
    steps_ = preset.steps;
    nameEdit_->setText(preset.name);
    descriptionEdit_->setText(preset.description);
    m_currentFormRow = -1;
    refreshStepList();
    if (stepsList_->count() > 0)
        stepsList_->setCurrentRow(0);
    else
        rebuildParamForm();
}

void PresetEditorDialog::addStep(const QString& op) {
    if (steps_.size() >= BatchPresetSchema::kMaxSteps)
        return;
    BatchPresetStep step;
    step.op = op;
    // Schema defaults (the same the runner applies for absent params).
    if (op == QLatin1String("compress")) {
        step.params.insert(QStringLiteral("quality"), 75);
        step.params.insert(QStringLiteral("targetDpi"), 150);
    } else if (op == QLatin1String("watermark")) {
        step.params.insert(QStringLiteral("text"), QStringLiteral("CONFIDENTIAL"));
        step.params.insert(QStringLiteral("opacity"), 30);
    } else if (op == QLatin1String("pdfa-export")
               || op == QLatin1String("pdfa-check")) {
        step.params.insert(QStringLiteral("level"), QStringLiteral("2b"));
    } else if (op == QLatin1String("bates")) {
        step.params.insert(QStringLiteral("startNumber"), 1);
        step.params.insert(QStringLiteral("digitCount"), 6);
        step.params.insert(QStringLiteral("position"), QStringLiteral("bottom-right"));
    } else if (op == QLatin1String("strip-metadata")) {
        step.params.insert(QStringLiteral("clearInfoDict"), true);
        step.params.insert(QStringLiteral("sanitize"), true);
    }
    steps_.append(step);
    refreshStepList();
    stepsList_->setCurrentRow(steps_.size() - 1);
}

void PresetEditorDialog::removeCurrentStep() {
    const int row = stepsList_->currentRow();
    if (row < 0 || row >= steps_.size())
        return;
    steps_.removeAt(row);
    m_currentFormRow = -1;
    refreshStepList();
    if (!steps_.isEmpty())
        stepsList_->setCurrentRow(qBound(0, row, steps_.size() - 1));
    else
        rebuildParamForm();
}

bool PresetEditorDialog::moveCurrentStep(int delta) {
    const int row = stepsList_->currentRow();
    const int target = row + delta;
    if (row < 0 || row >= steps_.size() || target < 0 || target >= steps_.size())
        return false;
    // Read the current form back BEFORE the move (the form follows selection).
    if (paramForm_ && m_currentFormRow == row)
        applyParamFormToStep(&steps_[row]);
    steps_.swapItemsAt(row, target);
    m_currentFormRow = -1;
    refreshStepList();
    stepsList_->setCurrentRow(target);
    return true;
}

// The step rows carry the capability badges: every registry-gated step is
// queried; anything not Available is disclosed IN PLACE (amber for Degraded,
// red + "[unavailable]" for Unavailable*) with whyNot + alternative as the
// row tooltip. The row stays selectable — the design-time disclosure; the
// pre-flight refusal is the run-time half of the same rule.
void PresetEditorDialog::refreshStepList() {
    stepsList_->clear();
    for (int i = 0; i < steps_.size(); ++i) {
        const BatchPresetStep& step = steps_.at(i);
        const Capability cap = batchPresetStepCapability(step, capabilities_);
        QString text = QStringLiteral("%1. %2").arg(i + 1).arg(step.op);
        QColor color;
        if (cap.status == Availability::Degraded) {
            text += QStringLiteral(" — degraded");
            color = QColor("#c8a000");
        } else if (cap.status != Availability::Available) {
            text += QStringLiteral(" — [unavailable]");
            color = QColor("#c8442b");
        }
        auto* item = new QListWidgetItem(text, stepsList_);
        if (cap.status != Availability::Available) {
            item->setForeground(color);
            item->setToolTip(cap.whyNot + QStringLiteral("\n")
                             + tr("Supported alternative: ") + cap.alternative);
        }
    }
}

// The parameter form is rebuilt from the SAME widget families the batch
// panels use, CLAMPED to the schema ranges — the editor cannot type an
// out-of-range value (compress DPI 36-600, quality 1-100, watermark text
// <= 120 chars, bates digitCount 1-12, …).
void PresetEditorDialog::rebuildParamForm() {
    // Read the outgoing selection back first (never lose an edit).
    if (m_currentFormRow >= 0 && m_currentFormRow < steps_.size() && paramForm_)
        applyParamFormToStep(&steps_[m_currentFormRow]);
    m_currentFormRow = stepsList_->currentRow();

    if (paramForm_) {
        paramForm_->deleteLater();
        paramForm_ = nullptr;
    }
    if (m_currentFormRow < 0 || m_currentFormRow >= steps_.size()) {
        paramNote_->setText(tr("Select or add a step to edit its parameters."));
        return;
    }
    const BatchPresetStep& step = steps_.at(m_currentFormRow);
    const QVariantMap& p = step.params;

    auto makeSpin = [&p](const QString& key, int min, int max, int def,
                         QWidget* parent) {
        auto* spin = new QSpinBox(parent);
        spin->setObjectName(QStringLiteral("param_%1").arg(key));
        spin->setRange(min, max);
        spin->setValue(p.value(key, def).toInt());
        return spin;
    };
    auto makeEdit = [&p](const QString& key, int maxLen, QWidget* parent) {
        auto* edit = new QLineEdit(parent);
        edit->setObjectName(QStringLiteral("param_%1").arg(key));
        edit->setMaxLength(maxLen);
        edit->setText(p.value(key).toString());
        return edit;
    };

    auto* formWidget = new QWidget;
    auto* form = new QFormLayout(formWidget);
    if (step.op == QLatin1String("compress")) {
        auto* quality = new QSlider(Qt::Horizontal, formWidget);
        quality->setObjectName(QStringLiteral("param_quality"));
        quality->setRange(1, 100);
        quality->setValue(p.value(QStringLiteral("quality"), 75).toInt());
        form->addRow(tr("Quality (1-100):"), quality);
        form->addRow(tr("Target DPI (36-600):"),
                     makeSpin(QStringLiteral("targetDpi"), 36, 600, 150,
                              formWidget));
    } else if (step.op == QLatin1String("watermark")) {
        form->addRow(tr("Text (<= 120):"),
                     makeEdit(QStringLiteral("text"), 120, formWidget));
        form->addRow(tr("Opacity % (1-100):"),
                     makeSpin(QStringLiteral("opacity"), 1, 100, 30, formWidget));
    } else if (step.op == QLatin1String("pdfa-export")
               || step.op == QLatin1String("pdfa-check")) {
        auto* level = new QComboBox(formWidget);
        level->setObjectName(QStringLiteral("param_level"));
        for (const QString& v : { QStringLiteral("1b"), QStringLiteral("2b"),
                                  QStringLiteral("2u"), QStringLiteral("3b"),
                                  QStringLiteral("3u") })
            level->addItem(v);
        level->setCurrentText(
            p.value(QStringLiteral("level"), QStringLiteral("2b")).toString());
        form->addRow(tr("PDF/A level:"), level);
    } else if (step.op == QLatin1String("redact")) {
        auto* presets = new QLineEdit(formWidget);
        presets->setObjectName(QStringLiteral("param_presets"));
        presets->setPlaceholderText(tr("named preset keys, comma-separated"));
        presets->setText(p.value(QStringLiteral("presets")).toStringList()
                             .join(QStringLiteral(", ")));
        form->addRow(tr("Named PII presets:"), presets);
        auto* patterns = new QLineEdit(formWidget);
        patterns->setObjectName(QStringLiteral("param_patterns"));
        patterns->setPlaceholderText(tr("regex patterns, comma-separated"));
        patterns->setText(p.value(QStringLiteral("patterns")).toStringList()
                              .join(QStringLiteral(", ")));
        form->addRow(tr("Patterns:"), patterns);
    } else if (step.op == QLatin1String("strip-metadata")) {
        auto* clear = new QCheckBox(tr("Clear the Info dictionary"), formWidget);
        clear->setObjectName(QStringLiteral("param_clearInfoDict"));
        clear->setChecked(p.value(QStringLiteral("clearInfoDict"), true).toBool());
        form->addRow(clear);
        auto* sanitize = new QCheckBox(tr("Sanitize on save"), formWidget);
        sanitize->setObjectName(QStringLiteral("param_sanitize"));
        sanitize->setChecked(p.value(QStringLiteral("sanitize"), true).toBool());
        form->addRow(sanitize);
    } else if (step.op == QLatin1String("bates")) {
        form->addRow(tr("Prefix (<= 32):"),
                     makeEdit(QStringLiteral("prefix"), 32, formWidget));
        form->addRow(tr("Suffix (<= 32):"),
                     makeEdit(QStringLiteral("suffix"), 32, formWidget));
        form->addRow(tr("Start number:"),
                     makeSpin(QStringLiteral("startNumber"), 1, 2147483000, 1,
                              formWidget));
        form->addRow(tr("Digit count (1-12):"),
                     makeSpin(QStringLiteral("digitCount"), 1, 12, 6, formWidget));
        auto* position = new QComboBox(formWidget);
        position->setObjectName(QStringLiteral("param_position"));
        for (const QString& v : { QStringLiteral("bottom-right"),
                                  QStringLiteral("bottom-center"),
                                  QStringLiteral("bottom-left"),
                                  QStringLiteral("top-right"),
                                  QStringLiteral("top-center"),
                                  QStringLiteral("top-left") })
            position->addItem(v);
        position->setCurrentText(
            p.value(QStringLiteral("position"), QStringLiteral("bottom-right"))
                .toString());
        form->addRow(tr("Position:"), position);
    } else {
        paramNote_->setText(tr("Operation \"%1\" has no editable parameters.")
                                .arg(step.op));
    }
    paramForm_ = formWidget;
    paramScroll_->setWidget(formWidget);
    paramNote_->setText(QString());
}

void PresetEditorDialog::applyParamFormToStep(BatchPresetStep* step) {
    if (!paramForm_ || !step)
        return;
    const auto widget = [this](const QString& key) -> QWidget* {
        return paramForm_->findChild<QWidget*>(
            QStringLiteral("param_%1").arg(key));
    };
    const QString& op = step->op;
    QVariantMap p = step->params;
    if (op == QLatin1String("compress")) {
        if (auto* w = widget(QStringLiteral("quality")))
            p[QStringLiteral("quality")] = static_cast<QSlider*>(w)->value();
        if (auto* w = widget(QStringLiteral("targetDpi")))
            p[QStringLiteral("targetDpi")] = static_cast<QSpinBox*>(w)->value();
    } else if (op == QLatin1String("watermark")) {
        if (auto* w = widget(QStringLiteral("text")))
            p[QStringLiteral("text")] = static_cast<QLineEdit*>(w)->text();
        if (auto* w = widget(QStringLiteral("opacity")))
            p[QStringLiteral("opacity")] = static_cast<QSpinBox*>(w)->value();
    } else if (op == QLatin1String("pdfa-export")
               || op == QLatin1String("pdfa-check")) {
        if (auto* w = widget(QStringLiteral("level")))
            p[QStringLiteral("level")] = static_cast<QComboBox*>(w)->currentText();
    } else if (op == QLatin1String("redact")) {
        if (auto* w = widget(QStringLiteral("presets")))
            p[QStringLiteral("presets")] =
                static_cast<QLineEdit*>(w)->text().split(QLatin1Char(','),
                                                         Qt::SkipEmptyParts);
        if (auto* w = widget(QStringLiteral("patterns")))
            p[QStringLiteral("patterns")] =
                static_cast<QLineEdit*>(w)->text().split(QLatin1Char(','),
                                                         Qt::SkipEmptyParts);
    } else if (op == QLatin1String("strip-metadata")) {
        if (auto* w = widget(QStringLiteral("clearInfoDict")))
            p[QStringLiteral("clearInfoDict")] =
                static_cast<QCheckBox*>(w)->isChecked();
        if (auto* w = widget(QStringLiteral("sanitize")))
            p[QStringLiteral("sanitize")] =
                static_cast<QCheckBox*>(w)->isChecked();
    } else if (op == QLatin1String("bates")) {
        if (auto* w = widget(QStringLiteral("prefix")))
            p[QStringLiteral("prefix")] = static_cast<QLineEdit*>(w)->text();
        if (auto* w = widget(QStringLiteral("suffix")))
            p[QStringLiteral("suffix")] = static_cast<QLineEdit*>(w)->text();
        if (auto* w = widget(QStringLiteral("startNumber")))
            p[QStringLiteral("startNumber")] = static_cast<QSpinBox*>(w)->value();
        if (auto* w = widget(QStringLiteral("digitCount")))
            p[QStringLiteral("digitCount")] = static_cast<QSpinBox*>(w)->value();
        if (auto* w = widget(QStringLiteral("position")))
            p[QStringLiteral("position")] = static_cast<QComboBox*>(w)->currentText();
    }
    step->params = p;
}

bool PresetEditorDialog::savePreset() {
    errorLabel_->hide();
    errorLabel_->clear();
    // Read the visible form back into the currently selected step.
    if (m_currentFormRow >= 0 && m_currentFormRow < steps_.size() && paramForm_)
        applyParamFormToStep(&steps_[m_currentFormRow]);

    BatchPreset out = m_preset;
    out.name = nameEdit_->text().trimmed();
    out.description = descriptionEdit_->text().trimmed();
    out.steps = steps_;
    // Stamp creation/modification like the store's save boundary — a NEW
    // preset carries no timestamps yet (the store re-stamps `modified` on
    // the actual save).
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!out.created.isValid())
        out.created = now;
    if (!out.modified.isValid())
        out.modified = now;
    BatchPreset check = out;
    // A NEW preset has no id yet — the STORE assigns (and de-conflicts) the
    // slug on save. Validate the store's assignment rule with the slug the
    // store would derive from the name; the handed-back preset keeps the
    // empty id so the store's de-confliction stays authoritative.
    if (check.id.isEmpty())
        check.id = BatchPresetSchema::idFromName(check.name);
    QString err;
    // The editor cannot produce an invalid preset (clamped widgets) — an
    // impossible state is still refused honestly, never saved.
    if (!BatchPresetCodec::validate(check, &err)) {
        errorLabel_->setText(err);
        errorLabel_->show();
        return false;
    }
    m_preset = out;
    accept();
    return true;
}

} // namespace gp
