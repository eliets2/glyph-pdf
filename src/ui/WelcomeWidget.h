// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QList>

class QEvent;
class QGridLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QPaintEvent;

class WelcomeWidget : public QWidget {
    Q_OBJECT
public:
    explicit WelcomeWidget(QWidget* parent = nullptr);

    void setRecentFiles(const QStringList& files);

    // U01 testable seam: how many action-card columns fit into
    // `availableWidth` pixels given a per-card minimum width hint and the grid
    // gutter. Pure geometry math clamped to 1..3 columns; it consumes the
    // widget's real available width and real card size hints, never a
    // display-resolution assumption.
    static int columnsForWidth(int availableWidth, int cardMinWidth, int spacing);

    // R16: mark a task route as not available in this installation (optional
    // dependency absent, e.g. OCR models or the LibreOffice converter). The
    // card stays VISIBLE but disabled with the reason + alternative disclosed
    // on tooltip/statusTip/accessibleDescription — the same honesty contract
    // as the ribbon's planned entries (never a silent grey card).
    void setTaskAvailable(const QString& task, const QString& disclosure);

signals:
    void openFileRequested();
    void mergeFilesRequested();
    void importOfficeRequested();
    void imagesToPdfRequested();
    // R16 (PP07/UI03/UI04): the task-oriented welcome. One signal carries the
    // chosen task id ("edit", "convert", "ocr", "compress", "splitExtract",
    // "organize", "annotate", "fillSign", "protect" — open-dependent; and
    // "batch", "compare" — standalone tasks). The host preserves the intent
    // through file selection and applies it after the load.
    void taskRouteRequested(const QString& task);
    void recentFileRequested(const QString& filePath);
    void removeRecentFileRequested(const QString& filePath);

protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void reflowActionCards();
    void refreshRecentList();
    QString displayName(const QString& path) const;

    QStringList m_recentFiles;
    QList<QPushButton*> m_actionCards;  // creation order == visual/tab order
    QWidget*     m_viewport   = nullptr; // scroll-area viewport (width drives reflow)
    QWidget*     m_content    = nullptr; // scroll-area content
    QWidget*     m_container  = nullptr; // centered content column
    QGridLayout* m_cardsGrid  = nullptr;
    QListWidget* m_recentList = nullptr;
    QLabel*      m_recentEmpty = nullptr;
    int          m_columns    = 0;

    void onRecentItemClicked(const QString& path, bool exists);
};
