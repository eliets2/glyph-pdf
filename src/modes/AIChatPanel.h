// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QFutureWatcher>
#include <QList>
#include <memory>
#include "engines/ai/IAiProvider.h"

class QListWidget;
class QLineEdit;
class QToolButton;

namespace gp {

class AIChatPanel : public QFrame {
    Q_OBJECT
public:
    explicit AIChatPanel(QWidget* parent = nullptr);
    ~AIChatPanel() override;

private slots:
    void onSend();
    void onAiFinished();

private:
    IAiProvider* activeProvider() const;

    // AR-1 D5: disable input while a request is in-flight
    void setInputEnabled(bool enabled);

    std::unique_ptr<IAiProvider>               m_ollama;

    QListWidget*                               m_msgs    = nullptr;
    QLineEdit*                                 m_input   = nullptr;
    QToolButton*                               m_sendBtn = nullptr;

    QList<AiMessage>                           m_history;
    QFutureWatcher<AiResult>                   m_watcher;

    // AR-1 D5: row index replaces the void* QVariant property.
    // -1 = no request in-flight. We look up the item by row in onAiFinished
    // rather than storing a raw pointer that could dangle after list clear.
    int                                        m_cursorRow = -1;
};

} // namespace gp
