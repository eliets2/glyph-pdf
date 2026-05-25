#include "AIChatPanel.h"
#include "util/GpTheme.h"
#include "core/CredentialManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

AIChatPanel::AIChatPanel(QWidget* parent)
    : QFrame(parent),
      m_credentialManager(std::make_unique<CredentialManager>())
{
    setFixedWidth(Theme::AiPaneW);
    setObjectName("aiPanel");
    // border-left only on this frame, not descendants — use stylesheet inheritance scoped via objectName.
    // Note: target #aiPanel explicitly so child QFrames don't inherit the border.
    setStyleSheet("QFrame#aiPanel { border-left: 1px solid #393b40; }");

    const bool hasKey = m_credentialManager->hasKey(QStringLiteral("Anthropic"));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    // header
    auto* head = new QFrame; head->setProperty("role","modeToolbar"); head->setFixedHeight(32);
    auto* hr = new QHBoxLayout(head); hr->setContentsMargins(10,0,10,0); hr->setSpacing(6);
    auto* t = new QLabel(tr("AI ASSISTANT")); t->setStyleSheet("font-weight:600;letter-spacing:1.2px;");
    auto* m = new QLabel(hasKey ? tr("· Anthropic Claude · key configured")
                                : tr("· Not configured"));
    m->setProperty("mono",true);
    hr->addWidget(t); hr->addWidget(m); hr->addStretch(1);
    col->addWidget(head);

    auto* tabs = new QTabBar; tabs->setProperty("role","sideTabs");
    tabs->addTab(tr("CHAT")); tabs->addTab(tr("SUMMARY")); tabs->addTab(tr("SEARCH"));
    col->addWidget(tabs);

    // suggestions
    auto* sugg = new QFrame;
    auto* sr = new QHBoxLayout(sugg); sr->setContentsMargins(8,8,8,8); sr->setSpacing(6);
    for (auto s : QStringList{ tr("Summarize this document"), tr("Key financial figures?"), tr("Find all risk factors") }) {
        auto* b = new QToolButton; b->setText(s); b->setProperty("variant","ghost"); sr->addWidget(b);
    }
    sr->addStretch(1);
    col->addWidget(sugg);

    // messages
    auto* msgs = new QListWidget;
    if (hasKey) {
        msgs->addItem(QStringLiteral("Ready · API key configured. Real chat responses arrive in v1.1."));
    } else {
        msgs->addItem(QStringLiteral("Configure your Anthropic API key in Preferences › AI to enable chat."));
    }
    col->addWidget(msgs, 1);

    // input
    auto* inp = new QFrame; inp->setProperty("role","modeToolbar"); inp->setFixedHeight(48);
    auto* ir = new QHBoxLayout(inp); ir->setContentsMargins(8,8,8,8); ir->setSpacing(6);
    auto* le = new QLineEdit; le->setPlaceholderText(tr("Ask anything about this document…"));
    auto* send = new QToolButton; send->setText(QStringLiteral("\xe2\xae\x95")); send->setProperty("variant","accent");
    ir->addWidget(le, 1); ir->addWidget(send);
    col->addWidget(inp);

    // ── Wire send ───────────────────────────────────────────────────────
    auto sendHandler = [le, msgs]() {
        const auto userText = le->text().trimmed();
        if (userText.isEmpty()) return;
        msgs->addItem(QStringLiteral("YOU: ") + userText);
        msgs->addItem(QStringLiteral("AI: (v1.1) AI responses will appear here once real LLM calls are wired."));
        le->clear();
    };
    QObject::connect(send, &QToolButton::clicked, this, sendHandler);
    QObject::connect(le,   &QLineEdit::returnPressed, this, sendHandler);
}

AIChatPanel::~AIChatPanel() = default;

} // namespace gp
