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

    std::unique_ptr<IAiProvider>               m_ollama;

    QListWidget*                               m_msgs    = nullptr;
    QLineEdit*                                 m_input   = nullptr;
    QToolButton*                               m_sendBtn = nullptr;

    QList<AiMessage>                           m_history;
    QFutureWatcher<AiResult>                   m_watcher;
};

} // namespace gp
