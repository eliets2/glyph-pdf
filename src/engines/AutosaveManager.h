// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <memory>
#include <atomic>

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
    // AR-1 D4: member retry timer — child of `this` so it is auto-cancelled
    // when AutosaveManager is destroyed, eliminating the singleShot UAF.
    QTimer* m_retryTimer;
    int m_intervalSeconds = 300;
    // AR-1 D4: atomic so the prefetch-thread read in onTick is data-race-free.
    std::atomic<bool> m_saving{false};
};
