#include "AIChatPanel.h"
#include "util/GpTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

AIChatPanel::AIChatPanel(QWidget* parent) : QFrame(parent) {
    setFixedWidth(Theme::AiPaneW);
    setObjectName("aiPanel");
    // border-left only on this frame, not descendants — use stylesheet inheritance scoped via objectName.
    // Note: target #aiPanel explicitly so child QFrames don't inherit the border.
    setStyleSheet("QFrame#aiPanel { border-left: 1px solid #393b40; }");

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    // header
    auto* head = new QFrame; head->setProperty("role","modeToolbar"); head->setFixedHeight(32);
    auto* hr = new QHBoxLayout(head); hr->setContentsMargins(10,0,10,0); hr->setSpacing(6);
    auto* t = new QLabel("AI ASSISTANT"); t->setStyleSheet("font-weight:600;letter-spacing:1.2px;");
    auto* m = new QLabel("· Local · Llama 3.1 8B"); m->setProperty("mono",true);
    hr->addWidget(t); hr->addWidget(m); hr->addStretch(1);
    col->addWidget(head);

    auto* tabs = new QTabBar; tabs->setProperty("role","sideTabs");
    tabs->addTab("CHAT"); tabs->addTab("SUMMARY"); tabs->addTab("SEARCH");
    col->addWidget(tabs);

    // suggestions
    auto* sugg = new QFrame;
    auto* sr = new QHBoxLayout(sugg); sr->setContentsMargins(8,8,8,8); sr->setSpacing(6);
    for (auto s : QStringList{ "Summarize this document", "Key financial figures?", "Find all risk factors" }) {
        auto* b = new QToolButton; b->setText(s); b->setProperty("variant","ghost"); sr->addWidget(b);
    }
    sr->addStretch(1);
    col->addWidget(sugg);

    // messages
    auto* msgs = new QListWidget;
    msgs->addItem(QStringLiteral("YOU: What is the total revenue for Q4 2025?"));
    msgs->addItem(QStringLiteral("AI:  Q4 2025 total revenue was $2.418B, +14.2% YoY. [p.4] Growth driven by enterprise sales [p.6] and intl expansion [p.8]."));
    msgs->addItem(QStringLiteral("YOU: What are the main risk factors?"));
    msgs->addItem(QStringLiteral("AI:  ●●●  (writing…)"));
    col->addWidget(msgs, 1);

    // input
    auto* inp = new QFrame; inp->setProperty("role","modeToolbar"); inp->setFixedHeight(48);
    auto* ir = new QHBoxLayout(inp); ir->setContentsMargins(8,8,8,8); ir->setSpacing(6);
    auto* le = new QLineEdit; le->setPlaceholderText("Ask anything about this document…");
    auto* send = new QToolButton; send->setText("⮕"); send->setProperty("variant","accent");
    ir->addWidget(le, 1); ir->addWidget(send);
    col->addWidget(inp);
}

} // namespace gp
