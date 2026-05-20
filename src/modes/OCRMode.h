#pragma once
#include <QWidget>

class QComboBox;
class QCheckBox;
class QToolButton;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QFrame;
class QVBoxLayout;

namespace gp {

/// OCR Verification Mode: top toolbar + info strip + 4-pane splitter.
/// Session 9 additions: engine/strategy selector, preprocessing toggles,
/// confidence-coloring overlay, and "review before save" workflow.
class OCRMode : public QWidget {
    Q_OBJECT
public:
    explicit OCRMode(QWidget* parent = nullptr);

signals:
    void ocrRequested();
    void reviewAccepted();
    void reviewRejected();

private slots:
    void onRunOcr();
    void onAcceptResults();

private:
    void buildToolbar(QVBoxLayout* col);
    void buildInfoStrip(QVBoxLayout* col);
    void buildPanes(QVBoxLayout* col);
    void updateConfidenceOverlay();

    // Toolbar controls
    QComboBox*   m_engineCombo   = nullptr;
    QComboBox*   m_strategyCombo = nullptr;
    QComboBox*   m_langCombo     = nullptr;
    QCheckBox*   m_chkDeskew     = nullptr;
    QCheckBox*   m_chkBinarize   = nullptr;
    QCheckBox*   m_chkDenoise    = nullptr;
    QToolButton* m_btnRun        = nullptr;
    QToolButton* m_btnAccept     = nullptr;
    QToolButton* m_btnReject     = nullptr;

    // Info strip labels
    QLabel* m_lblPage       = nullptr;
    QLabel* m_lblAvgConf    = nullptr;
    QLabel* m_lblLowWords   = nullptr;
    QLabel* m_lblEngine     = nullptr;

    // Panes
    QListWidget*    m_pageList    = nullptr;
    QFrame*         m_imagePane   = nullptr;
    QPlainTextEdit* m_textEdit    = nullptr;
    QFrame*         m_zoomPane    = nullptr;
    QLabel*         m_zoomBig     = nullptr;
    QLabel*         m_zoomMeta    = nullptr;
};

} // namespace gp
