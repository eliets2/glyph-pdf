#include "CompareMode.h"
#include "util/GpTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QTextBrowser>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace gp {

CompareMode::CompareMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    // toolbar
    auto* tb = new QFrame;
    tb->setProperty("role","modeToolbar");
    tb->setFixedHeight(Theme::ToolbarH);
    auto* tr = new QHBoxLayout(tb);
    tr->setContentsMargins(10,0,10,0); tr->setSpacing(6);
    auto mono = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("mono",true); return l; };
    tr->addWidget(mono("COMPARE"));
    tr->addWidget(mono("Q4-Report-v1.pdf   ↔   Q4-Report-v2.pdf"));
    auto* prev = new QToolButton; prev->setText("← PREV"); prev->setProperty("variant","ghost"); tr->addWidget(prev);
    auto* next = new QToolButton; next->setText("NEXT →"); next->setProperty("variant","ghost"); tr->addWidget(next);
    tr->addWidget(mono("CHANGE 04 OF 23"));
    tr->addStretch(1);
    auto* close = new QToolButton; close->setText("Close Compare"); close->setProperty("variant","ghost"); tr->addWidget(close);
    col->addWidget(tb);

    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(1);

    auto buildSide = [](const QString& head, const QString& html) {
        auto* w = new QFrame;
        auto* l = new QVBoxLayout(w); l->setContentsMargins(0,0,0,0); l->setSpacing(0);
        auto* h = new QFrame;
        h->setProperty("role","modeToolbar"); h->setFixedHeight(24);
        auto* hr = new QHBoxLayout(h); hr->setContentsMargins(12,0,12,0);
        auto* hl = new QLabel(head); hl->setProperty("mono",true);
        hr->addWidget(hl); hr->addStretch(1);
        l->addWidget(h);
        auto* tb = new QTextBrowser;
        tb->setHtml(html);
        l->addWidget(tb, 1);
        return w;
    };

    split->addWidget(buildSide("Q4-Report-v1.pdf",
        "<div style='padding:20px;font-family:Manrope;color:#1a1a1a;background:#f4f1ea;'>"
        "<h2>Performance Overview</h2>"
        "<p>Consolidated revenue reached <span style='background:#c8442b22;text-decoration:line-through'>$2.318B, an increase of 12.8%</span> year-over-year.</p>"
        "<p>Operating margin <span style='background:#c8442b22;text-decoration:line-through'>contracted modestly by 40 basis points</span> to 21.6%.</p>"
        "<p>FCF generation of <span style='background:#c8442b22;text-decoration:line-through'>$542M</span> was modestly behind guidance.</p>"
        "</div>"));
    split->addWidget(buildSide("Q4-Report-v2.pdf",
        "<div style='padding:20px;font-family:Manrope;color:#1a1a1a;background:#f4f1ea;'>"
        "<h2>Performance Overview</h2>"
        "<p>Consolidated revenue reached <span style='background:#4ec9b022;text-decoration:underline'>$2.418B, an increase of 14.2%</span> year-over-year.</p>"
        "<p>Operating margin <span style='background:#4ec9b022;text-decoration:underline'>expanded by 180 basis points</span> to 22.4%.</p>"
        "<p>FCF generation of <span style='background:#4ec9b022;text-decoration:underline'>$612M</span> was ahead of guidance.</p>"
        "</div>"));

    col->addWidget(split, 1);

    // changes panel
    auto* changes = new QFrame;
    changes->setFixedHeight(160);
    auto* cl = new QVBoxLayout(changes); cl->setContentsMargins(0,0,0,0); cl->setSpacing(0);
    auto* ch = new QFrame; ch->setProperty("role","modeToolbar"); ch->setFixedHeight(26);
    auto* chr = new QHBoxLayout(ch); chr->setContentsMargins(12,0,12,0);
    auto* chl = new QLabel("23 CHANGES"); chl->setProperty("mono",true); chr->addWidget(chl); chr->addStretch(1);
    auto* chr2 = new QLabel("TEXT 18 · IMAGES 2 · FORMAT 3"); chr2->setProperty("mono",true); chr->addWidget(chr2);
    cl->addWidget(ch);

    auto* tree = new QTreeWidget;
    tree->setHeaderLabels({"#","Page","Description",""});
    tree->setRootIsDecorated(false);
    QStringList rows[] = {
        {"1","p.01","Revenue figure: $2.318B → $2.418B","JUMP"},
        {"2","p.01","Growth rate: 12.8% → 14.2%","JUMP"},
        {"3","p.01","Margin direction: contracted → expanded","JUMP"},
        {"4","p.01","FCF figure: $542M → $612M","JUMP"},
        {"5","p.01","Book-to-bill: 0.98× → 1.07×","JUMP"},
        {"6","p.04","Chart replaced: Revenue Q1–Q4","JUMP"},
        {"7","p.05","Heading style: H2 → H1","JUMP"},
    };
    for (auto& r : rows) new QTreeWidgetItem(tree, r);
    cl->addWidget(tree, 1);
    col->addWidget(changes);
}

} // namespace gp
