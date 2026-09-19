// SPDX-License-Identifier: Apache-2.0
#include "SigningProgressPanel.h"

#include "engines/SignatureManager.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

SigningProgressPanel::SigningProgressPanel(SignatureManager *signing,
                                           const QString &docPath, QWidget *parent)
    : QDialog(parent), m_signing(signing), m_docPath(docPath)
{
    // Modeless + dismissible: guidance, never a gate (P1 requirement).
    setWindowModality(Qt::NonModal);
    setWindowTitle(tr("Signing Request Progress"));
    resize(520, 380);

    auto *root = new QVBoxLayout(this);
    m_title = new QLabel(this);
    m_title->setWordWrap(true);
    root->addWidget(m_title);

    m_stateList = new QListWidget(this);
    root->addWidget(m_stateList, 1);

    m_disclosure = new QLabel(this);
    m_disclosure->setWordWrap(true);
    m_disclosure->setStyleSheet(QStringLiteral("color: #777;"));
    m_disclosure->setText(SigningRequestModel::advisoryOrderDisclosure());
    root->addWidget(m_disclosure);

    auto *buttons = new QHBoxLayout;
    m_signBtn = new QPushButton(tr("Sign as this signer"), this);
    auto *verifyBtn = new QPushButton(tr("Verify Signatures"), this);
    auto *closeBtn = new QPushButton(tr("Dismiss"), this);
    buttons->addWidget(m_signBtn);
    buttons->addWidget(verifyBtn);
    buttons->addStretch(1);
    buttons->addWidget(closeBtn);
    root->addLayout(buttons);

    connect(m_signBtn, &QPushButton::clicked, this, [this] {
        const int cur = m_model.currentSignerIndex();
        if (cur < m_model.signers.size()) emit signRequested(cur);
    });
    connect(verifyBtn, &QPushButton::clicked, this, [this] {
        refresh();
        emit verifyRequested();
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

    refresh();
}

QString SigningProgressPanel::buildStatusText(
    const SigningRequestModel &model,
    const SigningRequestRunner::VerificationReport &report)
{
    QString text;
    if (model.isEmpty()) {
        text = QStringLiteral("This document has an empty signing request.");
        return text;
    }
    if (model.isComplete()) {
        text = QStringLiteral("Signing request complete — all %1 signer(s) signed. "
                              "%2")
                   .arg(model.signers.size())
                   .arg(report.consistent
                            ? QStringLiteral("Every recorded signature matches the document.")
                            : report.warnings.join(QStringLiteral(" ")));
        return text;
    }
    const int cur = model.currentSignerIndex();
    const SigningRequestModel::Signer &s = model.signers[cur];
    text = QStringLiteral("Signer %1 of %2: %3 — field %4%5")
               .arg(cur + 1)
               .arg(model.signers.size())
               .arg(s.name)
               .arg(s.fieldName)
               .arg(s.anchorPage >= 0
                        ? QStringLiteral(" (page %1)").arg(s.anchorPage + 1)
                        : QString());
    for (const QString &w : report.warnings)
        text += QStringLiteral("\nWarning: %1").arg(w);
    return text;
}

bool SigningProgressPanel::refresh()
{
    const auto loaded =
        SigningRequestModel::load(SigningRequestModel::sidecarPathFor(m_docPath));
    if (loaded.error != SigningRequestModel::LoadError::None) {
        m_model = SigningRequestModel();
        m_report = SigningRequestRunner::VerificationReport();
        m_title->setText(tr("The signing request could not be read and was NOT used "
                            "(%1)").arg(loaded.detail));
        m_stateList->clear();
        m_signBtn->setEnabled(false);
        return false;
    }
    m_model = loaded.model;
    if (m_signing)
        m_report = SigningRequestRunner::verifyAgainstDocument(*m_signing, m_model, m_docPath);
    else
        m_report = SigningRequestRunner::VerificationReport();
    rebuild();
    return true;
}

void SigningProgressPanel::rebuild()
{
    m_title->setText(buildStatusText(m_model, m_report));
    m_stateList->clear();
    for (int i = 0; i < m_model.signers.size(); ++i) {
        const SigningRequestModel::Signer &s = m_model.signers[i];
        QString line;
        if (s.isSigned) {
            // ONLY the engine-attested facts, exactly as recorded by the step.
            line = tr("%1. %2 — SIGNED at %3 (field %4, PAdES %5) — %6")
                       .arg(i + 1)
                       .arg(s.name)
                       .arg(s.signedAtUtc)
                       .arg(s.signedFieldName.isEmpty() ? s.fieldName : s.signedFieldName)
                       .arg(s.attainedLevel.isEmpty() ? QStringLiteral("?") : s.attainedLevel)
                       .arg(s.signatureSummary);
            if (!s.fieldMatch)
                line += tr(" [note: a different field received this signature than the "
                           "one bound — recorded as-is]");
        } else {
            const bool current = (i == m_model.currentSignerIndex());
            line = tr("%1. %2 — pending%3 (field %4%5)")
                       .arg(i + 1)
                       .arg(s.name)
                       .arg(current ? tr(" — CURRENT SIGNER") : QString())
                       .arg(s.fieldName)
                       .arg(s.anchorPage >= 0 ? tr(", page %1").arg(s.anchorPage + 1)
                                              : QString());
        }
        m_stateList->addItem(line);
    }
    const bool canSign = !m_model.isEmpty() && !m_model.isComplete();
    m_signBtn->setEnabled(canSign);
}
