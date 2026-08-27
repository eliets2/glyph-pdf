// SPDX-License-Identifier: Apache-2.0
#include "UpdateDialog.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace gp {

UpdateDialog::UpdateDialog(UpdateChecker* checker,
                           const UpdateChecker::UpdateInfo& info,
                           QWidget* parent)
    : QDialog(parent), m_checker(checker)
{
    setWindowTitle(tr("Software Update"));
    setModal(true);
    setMinimumWidth(440);
    setProperty("role", "modal");

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(28, 24, 28, 20);
    col->setSpacing(10);

    auto* title = new QLabel(tr("A new version of GlyphPDF is available"));
    title->setStyleSheet("font-size:15px; font-weight:700;");
    col->addWidget(title);

    auto* ver = new QLabel(QStringLiteral("GlyphPDF %1").arg(info.version));
    ver->setStyleSheet("font-size:26px; font-weight:800; color:#ff8c42;");
    col->addWidget(ver);

    auto* sub = new QLabel(tr("Installed: %1   ·   Released: %2")
                               .arg(UpdateChecker::currentVersion(),
                                    info.releaseDate.isEmpty() ? tr("unknown") : info.releaseDate));
    sub->setStyleSheet("color:#9aa0aa; font-size:12px;");
    col->addWidget(sub);

    // Release-notes link (manifest provides a URL).
    if (!info.releaseNotes.isEmpty()) {
        auto* notes = new QPushButton(tr("View release notes ↗"));
        notes->setCursor(Qt::PointingHandCursor);
        notes->setStyleSheet(
            "text-align:left; color:#8aa6d8; background:transparent; border:none; padding:2px 0;");
        const QString url = info.releaseNotes;
        connect(notes, &QPushButton::clicked, this, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        col->addWidget(notes);
    }

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#2c3340;");
    col->addWidget(sep);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setVisible(false);
    col->addWidget(m_progress);

    m_status = new QLabel(tr("The download is verified (SHA-256 and signature) before install. "
                             "Nothing is installed without your confirmation."));
    m_status->setStyleSheet("color:#9aa0aa; font-size:11px;");
    m_status->setWordWrap(true);
    col->addWidget(m_status);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    m_laterBtn = new QPushButton(tr("Later"));
    m_primaryBtn = new QPushButton(tr("Download"));
    m_primaryBtn->setDefault(true);
    m_primaryBtn->setStyleSheet(
        "background:#ff8c42; color:#15171c; font-weight:600; padding:7px 18px; border:none; border-radius:6px;");
    btnRow->addWidget(m_laterBtn);
    btnRow->addWidget(m_primaryBtn);
    col->addLayout(btnRow);

    // ── Button + checker wiring ──────────────────────────────────────────
    connect(m_laterBtn, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_primaryBtn, &QPushButton::clicked, this, [this]() {
        if (m_ready) {
            m_status->setText(tr("Launching installer…"));
            m_checker->applyUpdate();   // emits updateLaunched → app exits
            return;
        }
        m_primaryBtn->setEnabled(false);
        m_laterBtn->setEnabled(true);
        m_progress->setVisible(true);
        m_progress->setValue(0);
        m_status->setText(tr("Downloading update…"));
        m_checker->downloadUpdate();
    });

    connect(m_checker, &UpdateChecker::downloadProgressChanged, this, [this](int pct) {
        m_progress->setValue(pct);
        m_status->setText(tr("Downloading update… %1%").arg(pct));
    });

    connect(m_checker, &UpdateChecker::downloadReady, this, [this](const QString&) {
        m_ready = true;
        m_progress->setValue(100);
        m_status->setText(tr("✓ Downloaded and verified. Ready to install."));
        m_primaryBtn->setText(tr("Install && Restart"));
        m_primaryBtn->setEnabled(true);
    });

    connect(m_checker, &UpdateChecker::downloadFailed, this, [this](const QString& reason) {
        m_progress->setVisible(false);
        m_status->setText(tr("Download failed: %1").arg(reason));
        m_primaryBtn->setText(tr("Retry Download"));
        m_primaryBtn->setEnabled(true);
    });

    connect(m_checker, &UpdateChecker::updateLaunched, this, &QDialog::accept);
}

} // namespace gp
