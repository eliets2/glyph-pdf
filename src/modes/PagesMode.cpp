#include "PagesMode.h"
#include "util/GpTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

PagesMode::PagesMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    auto* tb = new QFrame; tb->setProperty("role","modeToolbar"); tb->setFixedHeight(Theme::ToolbarH);
    auto* tr = new QHBoxLayout(tb); tr->setContentsMargins(10,0,10,0); tr->setSpacing(4);
    auto mono = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("mono",true); return l; };
    tr->addWidget(mono("PAGES"));
    for (auto t : QStringList{ "Insert Before","Insert After","Delete","Extract","Replace","Rotate ↺","Rotate ↻","Split Here","Merge" }) {
        auto* b = new QToolButton; b->setText(t); b->setProperty("variant","ghost"); tr->addWidget(b);
    }
    tr->addStretch(1);
    for (auto s : QStringList{"S","M","L"}) {
        auto* b = new QToolButton; b->setText(s); b->setProperty("variant","pill"); b->setCheckable(true); b->setAutoExclusive(true); if (s=="M") b->setChecked(true); tr->addWidget(b);
    }
    col->addWidget(tb);

    auto* split = new QSplitter(Qt::Horizontal);
    auto* grid = new QListWidget;
    grid->setViewMode(QListView::IconMode);
    grid->setResizeMode(QListView::Adjust);
    grid->setSpacing(8);
    grid->setIconSize(QSize(140, 180));
    grid->setMovement(QListView::Snap);
    grid->setDragDropMode(QAbstractItemView::InternalMove);
    for (int i = 1; i <= 12; ++i) {
        auto* it = new QListWidgetItem(QString("page %1\n%2").arg(QString::number(i).rightJustified(3,'0'), QStringList{"Cover","Contents","Exec","Q4 Perf","Revenue","Geo","Op Margin","Outlook","Risk","Appx A","Appx B","Glossary"}.value(i-1)));
        it->setSizeHint(QSize(180, 220));
        grid->addItem(it);
    }
    grid->setCurrentRow(3);
    split->addWidget(grid);

    auto* panel = new QFrame; panel->setFixedWidth(260);
    auto* pl = new QVBoxLayout(panel); pl->setContentsMargins(0,0,0,0); pl->setSpacing(0);
    auto* ph = new QFrame; ph->setProperty("role","modeToolbar"); ph->setFixedHeight(26);
    auto* phr = new QHBoxLayout(ph); phr->setContentsMargins(12,0,12,0);
    auto* phl = new QLabel("SPLIT DOCUMENT"); phl->setProperty("mono",true); phr->addWidget(phl); phr->addStretch(1);
    pl->addWidget(ph);
    auto* placeholder = new QLabel("Form layout: SPLIT AT radio,\nN PAGES input,\nNAMING pattern,\nDESTINATION,\nPreview · Split");
    placeholder->setProperty("mono", true);
    placeholder->setMargin(14);
    pl->addWidget(placeholder); pl->addStretch(1);
    split->addWidget(panel);

    col->addWidget(split, 1);
}

} // namespace gp
