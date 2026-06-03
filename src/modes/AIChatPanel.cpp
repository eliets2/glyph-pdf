// SPDX-License-Identifier: Apache-2.0
#include "AIChatPanel.h"
#include "util/GpTheme.h"
#include "engines/ai/OllamaProvider.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSettings>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

AIChatPanel::AIChatPanel(QWidget* parent)
    : QFrame(parent),
      m_ollama(std::make_unique<OllamaProvider>())
{
    setFixedWidth(Theme::AiPaneW);
    setObjectName("aiPanel");
    setStyleSheet("QFrame#aiPanel { border-left: 1px solid #393b40; }");

    const bool ready = m_ollama->isReady();

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────
    auto* head = new QFrame; head->setProperty("role","modeToolbar"); head->setFixedHeight(32);
    auto* hr = new QHBoxLayout(head); hr->setContentsMargins(10,0,10,0); hr->setSpacing(6);
    auto* t = new QLabel(tr("AI ASSISTANT")); t->setStyleSheet("font-weight:600;letter-spacing:1.2px;");
    auto* m = new QLabel(ready ? tr("· Ollama · ready")
                                : tr("· Ollama not reachable — start Ollama at localhost:11434"));
    m->setProperty("mono",true);
    hr->addWidget(t); hr->addWidget(m); hr->addStretch(1);
    col->addWidget(head);

    // ── Tabs ─────────────────────────────────────────────────────────────
    auto* tabs = new QTabBar; tabs->setProperty("role","sideTabs");
    tabs->addTab(tr("CHAT")); tabs->addTab(tr("SUMMARY")); tabs->addTab(tr("SEARCH"));
    col->addWidget(tabs);

    // ── Suggestion chips ─────────────────────────────────────────────────
    auto* sugg = new QFrame;
    auto* sr = new QHBoxLayout(sugg); sr->setContentsMargins(8,8,8,8); sr->setSpacing(6);
    const QStringList suggestions = {tr("Summarize this document"),
                                     tr("Key financial figures?"),
                                     tr("Find all risk factors")};
    for (const QString& s : suggestions) {
        auto* b = new QToolButton; b->setText(s); b->setProperty("variant","ghost");
        connect(b, &QToolButton::clicked, this, [this, s]() {
            if (m_input) m_input->setText(s);
            onSend();
        });
        sr->addWidget(b);
    }
    sr->addStretch(1);
    col->addWidget(sugg);

    // ── Message list ─────────────────────────────────────────────────────
    m_msgs = new QListWidget;
    m_msgs->setWordWrap(true);
    if (ready)
        m_msgs->addItem(tr("Ready · AI chat active. Type a question about the open document."));
    else
        m_msgs->addItem(tr("Ollama not reachable. Start Ollama at http://localhost:11434 to enable AI chat."));
    col->addWidget(m_msgs, 1);

    // ── Input row ─────────────────────────────────────────────────────────
    auto* inp = new QFrame; inp->setProperty("role","modeToolbar"); inp->setFixedHeight(48);
    auto* ir = new QHBoxLayout(inp); ir->setContentsMargins(8,8,8,8); ir->setSpacing(6);
    m_input = new QLineEdit; m_input->setPlaceholderText(tr("Ask anything about this document…"));
    m_sendBtn = new QToolButton;
    m_sendBtn->setText(QString::fromUtf8("\xe2\xae\x95"));
    m_sendBtn->setProperty("variant","accent");
    ir->addWidget(m_input, 1); ir->addWidget(m_sendBtn);
    col->addWidget(inp);

    connect(m_sendBtn, &QToolButton::clicked, this, &AIChatPanel::onSend);
    connect(m_input,   &QLineEdit::returnPressed, this, &AIChatPanel::onSend);
    connect(&m_watcher, &QFutureWatcher<AiResult>::finished, this, &AIChatPanel::onAiFinished);
}

AIChatPanel::~AIChatPanel() = default;

// ---------------------------------------------------------------------------

void AIChatPanel::onSend()
{
    if (!m_input || !m_msgs) return;
    const QString userText = m_input->text().trimmed();
    if (userText.isEmpty()) return;

    if (!m_ollama->isReady()) {
        m_msgs->addItem(tr("⚠ Ollama not reachable. Start Ollama at http://localhost:11434."));
        return;
    }

    // Show user message
    m_msgs->addItem(QStringLiteral("YOU: ") + userText);
    m_input->clear();
    m_sendBtn->setEnabled(false);

    // Typing cursor placeholder
    auto* cursor = new QListWidgetItem(tr("AI: …"), m_msgs);
    m_msgs->scrollToBottom();

    // Append to history
    m_history.append({QStringLiteral("user"), userText});

    // Submit async — non-blocking; QFutureWatcher::finished fires on main thread
    m_watcher.setFuture(m_ollama->chat(m_history));

    // Store pointer so onAiFinished can replace the placeholder
    m_msgs->setProperty("cursorItem", QVariant::fromValue(static_cast<void*>(cursor)));
}

void AIChatPanel::onAiFinished()
{
    m_sendBtn->setEnabled(true);

    const AiResult result = m_watcher.result();

    // Replace the typing-cursor placeholder
    auto* cursor = static_cast<QListWidgetItem*>(
        m_msgs->property("cursorItem").value<void*>());
    if (cursor) {
        const QString display = result.ok
            ? (QStringLiteral("AI: ") + result.text)
            : (QStringLiteral("AI ⚠: ") + result.errorMsg);
        cursor->setText(display);
    }
    m_msgs->scrollToBottom();
    m_msgs->setProperty("cursorItem", QVariant());

    if (result.ok) {
        // Append assistant turn to history
        m_history.append({QStringLiteral("assistant"), result.text});
    }
}

} // namespace gp
