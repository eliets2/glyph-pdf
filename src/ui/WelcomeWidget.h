#pragma once
#include <QWidget>
#include <QMessageBox>

class WelcomeWidget : public QWidget {
    Q_OBJECT
public:
    explicit WelcomeWidget(QWidget* parent = nullptr);

    void setRecentFiles(const QStringList& files);

signals:
    void openFileRequested();
    void mergeFilesRequested();
    // Keep full signal set for existing MainWindow wiring
    void mergeRequested();
    void convertRequested();
    void protectRequested();
    void recentFileRequested(const QString& filePath);
    void removeRecentFileRequested(const QString& filePath);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void setupUi();
    void refreshRecentList();
    QString displayName(const QString& path) const;

    QStringList m_recentFiles;

    void onRecentItemClicked(const QString& path, bool exists);
};
