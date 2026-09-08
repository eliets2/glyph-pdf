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
    // EC02 (TEAM-ENGINE-CODE-REVIEW-2026-09-07): a queued autosave whose
    // captured document identity no longer matches at save/completion time.
    // Nothing was (or will be) written to the captured path's recovery file,
    // and no session was timestamped — the run was dropped as stale.
    void autosaveStale(const QString &capturedPath);

private slots:
    void onTick();

private:
    std::shared_ptr<IPdfEditorEngine> m_pdfEditor;
    std::shared_ptr<DocumentSession> m_document;
    QTimer* m_timer;
    int m_intervalSeconds = 300;
    std::atomic<bool> m_saving{false};
};
