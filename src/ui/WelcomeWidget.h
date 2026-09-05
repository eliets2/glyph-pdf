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

signals:
    void openFileRequested();
    void mergeFilesRequested();
    void convertRequested();
    void protectRequested();
    void importOfficeRequested();
    void imagesToPdfRequested();
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
