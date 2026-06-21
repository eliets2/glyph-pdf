// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include "core/UpdateChecker.h"

class QLabel;
class QProgressBar;
class QPushButton;

namespace gp {

/// Modal "Software Update" dialog driven by a shared UpdateChecker.
/// Shows the new/installed versions, a release-notes link, a download progress
/// bar, and a primary button that walks Download → (SHA-256 verified) →
/// Install & Restart. The UpdateChecker is owned by the caller (MainWindow).
class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    UpdateDialog(UpdateChecker* checker,
                 const UpdateChecker::UpdateInfo& info,
                 QWidget* parent = nullptr);

private:
    UpdateChecker*              m_checker;
    UpdateChecker::UpdateInfo   m_info;
    QLabel*       m_status     = nullptr;
    QProgressBar* m_progress   = nullptr;
    QPushButton*  m_primaryBtn = nullptr;   // Download → Install & Restart
    QPushButton*  m_laterBtn   = nullptr;
    bool          m_ready      = false;
};

} // namespace gp
