// SPDX-License-Identifier: Apache-2.0
#include "SigningRequestDialog.h"

#include "core/SigningRequestRunner.h"
#include "engines/SignatureManager.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
constexpr int kNewFieldRole = Qt::UserRole;          // QString fieldName ("" = new)
constexpr char kCreatedRole[] = "createdField";       // bool data(1) on entries
}

SigningRequestDialog::SigningRequestDialog(SignatureManager *signing,
                                           const QString &docPath, QWidget *parent)
    : QDialog(parent), m_signing(signing), m_docPath(docPath)
{
    setWindowTitle(tr("Prepare Signing Request"));
    setModal(true);
    resize(620, 420);

    // Document inventory: every signature field in document order + the set
    // that already carries a signature (those must never be bound — one
    // field carries exactly one signature).
    if (m_signing) {
        const auto anchors = m_signing->signatureFieldAnchors(docPath);
        for (const auto &a : anchors) m_documentFieldOrder << a.fieldName;
        for (const SignatureInfo &info : m_signing->validateSignatures(docPath))
            if (info.isValid || info.integrityIntact)
                m_signedFields.insert(info.fieldName);
    }

    auto *root = new QVBoxLayout(this);
    auto *body = new QHBoxLayout;
    m_signerList = new QListWidget(this);
    m_signerList->setMinimumWidth(180);
    body->addWidget(m_signerList, 1);

    auto *form = new QFormLayout;
    m_nameEdit = new QLineEdit(this);
    form->addRow(tr("Signer name"), m_nameEdit);

    m_bindingCombo = new QComboBox(this);
    for (const QString &f : std::as_const(m_documentFieldOrder)) {
        if (m_signedFields.contains(f)) continue; // never offer signed fields
        m_bindingCombo->addItem(tr("Field: %1").arg(f), f);
    }
    m_bindingCombo->addItem(tr("[New signature field at anchor…]"), QString());
    form->addRow(tr("Signature field"), m_bindingCombo);

    m_anchorWidget = new QWidget(this);
    auto *anchorForm = new QFormLayout(m_anchorWidget);
    anchorForm->setContentsMargins(0, 0, 0, 0);
    m_pageSpin = new QSpinBox(m_anchorWidget);
    m_pageSpin->setRange(0, 9999);
    m_pageSpin->setPrefix(tr("page "));
    // 0-based internal, 1-based display handled by the label wording below.
    m_xSpin = new QDoubleSpinBox(m_anchorWidget);
    m_ySpin = new QDoubleSpinBox(m_anchorWidget);
    m_wSpin = new QDoubleSpinBox(m_anchorWidget);
    m_hSpin = new QDoubleSpinBox(m_anchorWidget);
    for (QDoubleSpinBox *s : { m_xSpin, m_ySpin, m_wSpin, m_hSpin }) {
        s->setRange(0.0, 20000.0);
        s->setDecimals(1);
        s->setSuffix(tr(" pt"));
    }
    m_xSpin->setValue(72.0);
    m_ySpin->setValue(72.0);
    m_wSpin->setValue(150.0);
    m_hSpin->setValue(60.0);
    anchorForm->addRow(tr("Anchor page (0-based)"), m_pageSpin);
    anchorForm->addRow(tr("Anchor x / y"), m_xSpin);
    anchorForm->addRow(tr("Anchor width / height"), m_wSpin);
    m_anchorWidget->setEnabled(false);
    form->addRow(m_anchorWidget);

    body->addLayout(form, 2);
    root->addLayout(body);

    auto *buttons = new QHBoxLayout;
    auto *addBtn = new QPushButton(tr("Add Signer"), this);
    auto *removeBtn = new QPushButton(tr("Remove"), this);
    auto *upBtn = new QPushButton(tr("Up"), this);
    auto *downBtn = new QPushButton(tr("Down"), this);
    buttons->addWidget(addBtn);
    buttons->addWidget(removeBtn);
    buttons->addWidget(upBtn);
    buttons->addWidget(downBtn);
    buttons->addStretch(1);
    root->addLayout(buttons);

    m_disclosure = new QLabel(this);
    m_disclosure->setWordWrap(true);
    m_disclosure->setStyleSheet(QStringLiteral("color: #777;"));
    root->addWidget(m_disclosure);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(tr("Save Request"));
    root->addWidget(box);

    // Disclosure 1 (scope cut D3): the order is guidance, never enforcement.
    // Disclosure 2: what "[New signature field]" really does (real /FT /Sig
    // field created at the anchor — the Form Builder "signature" button is a
    // TEXT box, so the honest creation route is this flow's own).
    m_disclosure->setText(tr("%1\n\n\"[New signature field at anchor…]\" records an anchor; "
                             "the real signature field (/FT /Sig) is placed at that anchor "
                             "when the signer's own step runs — the engine refuses to sign "
                             "while later signers' empty fields exist. The engine fills the "
                             "first unsigned signature field; after each step the request "
                             "records which field actually received the signature.")
                              .arg(SigningRequestModel::advisoryOrderDisclosure()));

    connect(addBtn, &QPushButton::clicked, this, [this] {
        SigningRequestModel::Signer s;
        s.name = tr("Signer %1").arg(m_model.signers.size() + 1);
        // Default binding: first free unsigned field, else a new anchored field.
        QStringList taken = m_model.boundFieldNames();
        taken << m_documentFieldOrder;
        bool bound = false;
        for (const QString &f : std::as_const(m_documentFieldOrder)) {
            if (m_signedFields.contains(f)) continue;
            if (!m_model.boundFieldNames().contains(f)) {
                s.fieldName = f;
                bound = true;
                break;
            }
        }
        if (!bound) {
            s.fieldName = uniqueFieldName(taken, QStringLiteral("sig"));
            s.createdField = true;
            s.anchorPage = 0;
            s.anchorRect = QRectF(72.0, 72.0, 150.0, 60.0);
        }
        addSigner(s);
    });
    connect(removeBtn, &QPushButton::clicked, this, [this] {
        const int row = m_signerList->currentRow();
        if (row >= 0) removeSigner(row);
    });
    connect(upBtn, &QPushButton::clicked, this, [this] {
        const int row = m_signerList->currentRow();
        if (row > 0) moveSigner(row, row - 1);
    });
    connect(downBtn, &QPushButton::clicked, this, [this] {
        const int row = m_signerList->currentRow();
        if (row >= 0 && row < m_model.signers.size() - 1) moveSigner(row, row + 1);
    });
    connect(m_bindingCombo, &QComboBox::currentIndexChanged, this, [this] {
        m_anchorWidget->setEnabled(m_bindingCombo->currentData().toString().isEmpty());
    });
    connect(m_signerList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_prevRow >= 0 && m_prevRow < m_model.signers.size()) loadFromUi(m_prevRow);
        m_prevRow = row;
        refreshUi();
    });
    connect(box, &QDialogButtonBox::accepted, this, [this] {
        QString err;
        if (saveRequest(&err)) accept();
        else if (!err.isEmpty())
            m_disclosure->setText(err);
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (m_model.signers.isEmpty()) {
        addBtn->click();   // start with one signer — the common case
    }
    m_signerList->setCurrentRow(0);
    refreshUi();
}

QString SigningRequestDialog::uniqueFieldName(const QStringList &taken, const QString &prefix)
{
    int n = 1;
    for (;;) {
        const QString candidate = prefix + QString::number(n);
        if (!taken.contains(candidate)) return candidate;
        ++n;
    }
}

QString SigningRequestDialog::orderDeviationWarning(const QStringList &requestedOrder,
                                                    const QStringList &documentFieldOrder)
{
    // Document order restricted to the requested fields.
    QStringList docRestricted;
    for (const QString &f : documentFieldOrder)
        if (requestedOrder.contains(f)) docRestricted << f;
    if (docRestricted == requestedOrder) return {};
    return QStringLiteral(
        "Note: the request's signer order differs from the document's field order "
        "(document order: %1). The engine fills the first unsigned field in document "
        "order; each step records the field that actually received the signature.")
        .arg(docRestricted.join(QStringLiteral(", ")));
}

void SigningRequestDialog::addSigner(const SigningRequestModel::Signer &signer)
{
    loadFromUi(m_signerList->currentRow());   // flush any in-form edits first
    m_model.signers.append(signer);
    syncEntryList();
    m_signerList->setCurrentRow(m_model.signers.size() - 1);
    refreshUi();   // setCurrentRow is a no-op when the row is unchanged — sync anyway
}

void SigningRequestDialog::removeSigner(int index)
{
    if (index < 0 || index >= m_model.signers.size()) return;
    m_model.signers.remove(index);
    syncEntryList();
    if (!m_model.signers.isEmpty())
        m_signerList->setCurrentRow(qBound(0, index, m_model.signers.size() - 1));
    refreshUi();
}

void SigningRequestDialog::moveSigner(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_model.signers.size()) return;
    if (toIndex < 0 || toIndex >= m_model.signers.size()) return;
    m_model.signers.move(fromIndex, toIndex);
    syncEntryList();
    m_signerList->setCurrentRow(toIndex);
    refreshUi();   // setCurrentRow is a no-op when the row is unchanged — sync anyway
}

void SigningRequestDialog::syncEntryList()
{
    // Preserve the current selection across rebuilds where possible.
    const int cur = m_signerList ? m_signerList->currentRow() : -1;
    QSignalBlocker block(m_signerList);
    m_signerList->clear();
    for (int i = 0; i < m_model.signers.size(); ++i) {
        const SigningRequestModel::Signer &s = m_model.signers[i];
        const QString state = s.isSigned ? tr(" — signed") : QString();
        m_signerList->addItem(tr("%1. %2 (%3)%4").arg(i + 1).arg(s.name, s.fieldName, state));
    }
    if (cur >= 0 && cur < m_signerList->count())
        m_signerList->setCurrentRow(cur);
}

void SigningRequestDialog::refreshUi()
{
    const int row = m_signerList->currentRow();
    m_prevRow = row;   // the form now shows exactly this entry
    if (row < 0 || row >= m_model.signers.size()) {
        m_nameEdit->clear();
        m_bindingCombo->setEnabled(false);
        m_anchorWidget->setEnabled(false);
        return;
    }
    m_bindingCombo->setEnabled(true);
    const SigningRequestModel::Signer &s = m_model.signers[row];
    m_nameEdit->setText(s.name);
    // Binding combo: select the entry's field when it exists among the items.
    const int fieldIdx = m_bindingCombo->findData(s.fieldName);
    const bool isNew = s.createdField || fieldIdx < 0;
    if (isNew) {
        m_bindingCombo->setCurrentIndex(m_bindingCombo->count() - 1); // "[new…]"
        if (s.anchorPage >= 0) {
            m_pageSpin->setValue(s.anchorPage);
            m_xSpin->setValue(s.anchorRect.x());
            m_ySpin->setValue(s.anchorRect.y());
            m_wSpin->setValue(s.anchorRect.width());
            m_hSpin->setValue(s.anchorRect.height());
        }
        m_anchorWidget->setEnabled(true);
    } else {
        m_bindingCombo->setCurrentIndex(fieldIdx);
        m_anchorWidget->setEnabled(false);
    }
}

void SigningRequestDialog::loadFromUi(int index)
{
    if (index < 0 || index >= m_model.signers.size()) return;
    SigningRequestModel::Signer &s = m_model.signers[index];
    s.name = m_nameEdit->text().trimmed();
    const QString boundField = m_bindingCombo->currentData().toString();
    if (boundField.isEmpty()) {
        // "[New signature field at anchor…]"
        if (s.fieldName.isEmpty())
            s.fieldName = uniqueFieldName(m_model.boundFieldNames()
                                              << m_documentFieldOrder,
                                          QStringLiteral("sig"));
        s.createdField = true;
        s.anchorPage = m_pageSpin->value();
        s.anchorRect = QRectF(m_xSpin->value(), m_ySpin->value(),
                              m_wSpin->value(), m_hSpin->value());
    } else {
        s.fieldName = boundField;
        s.createdField = false;
        s.anchorPage = -1;
        s.anchorRect = QRectF();
    }
}

bool SigningRequestDialog::saveRequest(QString *err)
{
    // Flush the currently edited entry into the model first.
    loadFromUi(m_signerList->currentRow());

    auto fail = [err](const QString &msg) {
        if (err) *err = msg;
        return false;
    };
    if (!m_signing) return fail(tr("No signing engine available."));
    if (m_docPath.isEmpty() || !QFileInfo::exists(m_docPath))
        return fail(tr("The document does not exist."));
    if (m_model.signers.isEmpty())
        return fail(tr("Add at least one signer before saving the request."));

    // ── Validation (fail-loud; nothing is written on any refusal) ──────────
    QStringList takenNames = m_documentFieldOrder;
    for (int i = 0; i < m_model.signers.size(); ++i) {
        SigningRequestModel::Signer &s = m_model.signers[i];
        if (s.name.isEmpty())
            return fail(tr("Signer %1 has no name.").arg(i + 1));
        if (s.fieldName.isEmpty())
            return fail(tr("Signer %1 has no signature field binding.").arg(i + 1));
        if (!s.createdField && !m_documentFieldOrder.contains(s.fieldName))
            return fail(tr("Signer %1 is bound to field %2, which does not exist in the "
                           "document.")
                            .arg(i + 1).arg(s.fieldName));
        if (!s.createdField && m_signedFields.contains(s.fieldName))
            return fail(tr("Signer %1 is bound to field %2, which already carries a "
                           "signature — one field carries exactly one signature.")
                            .arg(i + 1).arg(s.fieldName));
        // Re-run uniqueness INCLUDING entry names so two entries never share a
        // field (the creator also refuses duplicates — belt and braces).
        if (takenNames.count(s.fieldName) > (s.createdField ? 0 : 1))
            return fail(tr("Signature field %1 is bound more than once.").arg(s.fieldName));
        takenNames << s.fieldName;
    }

    // ── Bind the request to the PREPARED bytes and persist ──────────────────
    // NOTE: anchored fields are NOT created here — LAZY PLACEMENT. The
    // engine's post-condition refuses every sign whose document still contains
    // an unsigned signature field, so each fill step creates exactly ITS OWN
    // field right before signing (SigningRequestRunner::runFillStep). The
    // sidecar carries the anchors until then; the saved document bytes are
    // untouched by preparation.
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (m_model.createdUtc.isEmpty()) m_model.createdUtc = now;
    m_model.preparedUtc = now;
    m_model.preparedSha256 = SigningRequestRunner::documentSha256(m_docPath);
    m_model.reconfirmedSha256.clear();
    m_model.sourcePdfName = QFileInfo(m_docPath).fileName();
    QString saveErr;
    if (!m_model.save(SigningRequestModel::sidecarPathFor(m_docPath), &saveErr))
        return fail(saveErr);

    emit requestSaved(false /* fieldsCreated — placement is lazy now */);
    return true;
}
