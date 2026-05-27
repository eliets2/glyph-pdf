#pragma once

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <memory>

class IPdfEditorEngine;
class DocumentSession;

class AutosaveManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AutosaveManager)

public:
    explicit AutosaveManager(std::shared_ptr<IPdfEditorEngine> pdfEditor, std::shared_ptr<DocumentSession> document, QObject* parent = nullptr);
    ~AutosaveManager() override;

    void start(int intervalSeconds = 300);
    void stop();
    bool isActive() const;
    int interval() const;

signals:
    void autosaveStarted();
    void autosaveCompleted(const QDateTime &time);
    void autosaveFailed(const QString &reason);

private slots:
    void onTick();

private:
    std::shared_ptr<IPdfEditorEngine> m_pdfEditor;
    std::shared_ptr<DocumentSession> m_document;
    QTimer* m_timer;
    int m_intervalSeconds = 300;
    bool m_saving = false;
};
