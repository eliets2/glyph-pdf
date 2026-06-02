// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QList>

struct AppContext;
class QListWidget;
class QSpinBox;
class QLineEdit;
class QLabel;
class QListWidgetItem;
class QRadioButton;
class QComboBox;

namespace gp {

class PagesMode : public QWidget {
    Q_OBJECT
public:
    explicit PagesMode(QWidget* parent = nullptr);

    // Called by ModeController after construction (same pattern as BatchMode).
    void setAppContext(const AppContext* ctx);

    // Refresh page list and split-form state when a document is opened/closed.
    void refreshPageList();

    // --- Test seams (headless, no display required) ---
    // Parse "1-3,5,7-9" into 0-based indices [0,1,2,4,6,7,8].
    static QList<int> parsePageRange(const QString& expr, int pageCount);

    // Execute split without UI: returns paths of produced files.
    QStringList executeSplit(const QString& sourcePath,
                             const QList<QList<int>>& groups,
                             const QString& outputDir,
                             const QString& stemPattern);

    // Write a minimal valid one-page PDF stub (used by executeSplit + tests).
    static bool writeMinimalPdf(const QString& path);

private slots:
    void onPreviewSplit();
    void onSplit();
    void onApplyReorder();
    void onResetReorder();
    void onThumbnailSizeChanged();

private:
    // Build sub-widgets
    void buildPageListPanel(QWidget* host);
    void buildSplitPanel(QWidget* host);
    void buildReorderPanel(QWidget* host);

    // Compute split groups from current form state.
    QList<QList<int>> computeSplitGroups() const;
    // Build output filename for part n (1-based) from pattern and stem.
    QString makeOutputName(const QString& pattern, const QString& stem, int part) const;
    // Page list (D1)
    QListWidget*  m_pageList    = nullptr;
    QLabel*       m_pageCountLabel = nullptr;

    // Split form (D2)
    QRadioButton* m_splitAtRadio    = nullptr;
    QRadioButton* m_splitEveryRadio = nullptr;
    QRadioButton* m_splitRangeRadio = nullptr;
    QSpinBox*     m_splitAtSpin     = nullptr;   // split at page N
    QSpinBox*     m_splitEverySpin  = nullptr;   // split every N pages
    QLineEdit*    m_splitRangeEdit  = nullptr;   // "1-5,7,9-12" expression
    QLineEdit*    m_namingEdit      = nullptr;
    QLineEdit*    m_outDirEdit      = nullptr;
    QListWidget*  m_previewList     = nullptr;   // filename preview

    // Reorder panel (D3)
    QListWidget*  m_reorderList     = nullptr;
    QList<int>    m_originalOrder;  // 0-based original page indices

    const AppContext* m_ctx = nullptr;
};

} // namespace gp
