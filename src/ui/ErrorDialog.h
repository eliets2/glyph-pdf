#pragma once

#include "core/ErrorInfo.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QTextEdit;
QT_END_NAMESPACE

namespace gp {

/**
 * Modal dialog that presents an ErrorInfo to the user.
 *
 * Buttons shown depend on ErrorInfo::suggestedActions:
 *   Retry     → retryRequested()  / QDialog::Accepted
 *   Skip      → skipRequested()   / result code SkipResult
 *   ExportLog → opens save-file dialog immediately
 *   OK        → always shown      / QDialog::Rejected (close)
 *
 * Technical details are hidden behind a "Show Details" toggle.
 */
class ErrorDialog : public QDialog
{
    Q_OBJECT
public:
    enum ResultCode { SkipResult = 100 };

    explicit ErrorDialog(const ErrorInfo& info, QWidget* parent = nullptr);

    // Convenience: show + exec in one call
    static int show(const ErrorInfo& info, QWidget* parent = nullptr);

    // Convenience: show a simple error with just OK
    static void showError(const QString& msg, QWidget* parent = nullptr);

signals:
    void retryRequested();
    void skipRequested();

private:
    void buildUi(const ErrorInfo& info);
    void onExportLog(const ErrorInfo& info);

    QLabel*      m_icon        = nullptr;
    QLabel*      m_message     = nullptr;
    QTextEdit*   m_details     = nullptr;
    QPushButton* m_toggleBtn   = nullptr;
    QPushButton* m_retryBtn    = nullptr;
    QPushButton* m_skipBtn     = nullptr;
    QPushButton* m_exportBtn   = nullptr;
    QPushButton* m_okBtn       = nullptr;
};

} // namespace gp
