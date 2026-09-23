// SPDX-License-Identifier: Apache-2.0
// ui-fix lane diagnostic (2026-09-20): per-screen width-budget breakdown.
// For each task screen, prints the window minimum width and the effective
// minimum-width contribution of every named region, so the F1 layout fix
// targets the real driver (not guesswork). Evidence goes to
// <out>/minwidth-<tag>.json. Production code is NOT modified.
//
// Usage: minwidth_probe.exe --out <dir> --tag <tag> [--width 1366 --height 768]

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QScrollArea>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>
#include <algorithm>
#include <functional>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "modes/ModeController.h"
#include "shell/Ribbon.h"
#include "shell/Sidebar.h"
#include "shell/StatusBar.h"
#include "shell/ModeStrip.h"
#include "shell/ScreenNav.h"

using gp::MainWindow;

namespace {

void drain(int ms = 300)
{
    QEventLoop e;
    QTimer::singleShot(ms, &e, &QEventLoop::quit);
    e.exec();
}

// What a layout actually reserves for w: an explicit minimumWidth (from
// setFixedWidth/setMinimumWidth) beats minimumSizeHint.
int effMinW(QWidget* w)
{
    if (!w) return -1;
    const int explicitMin = w->minimumWidth();
    return explicitMin > 0 ? explicitMin : w->minimumSizeHint().width();
}

// Vertical counterpart of effMinW, for the F1 height budget (768-high
// viewport: the harness grewPastRequested covers height too).
int effMinH(QWidget* w)
{
    if (!w) return -1;
    const int explicitMin = w->minimumHeight();
    return explicitMin > 0 ? explicitMin : w->minimumSizeHint().height();
}

QJsonArray wideLeaves(QWidget& root, int threshold, int limit = 12)
{
    struct Row { int w; QString path; };
    QList<Row> rows;
    std::function<void(QWidget*, const QString&)> walk =
        [&](QWidget* w, const QString& path) {
        if (!w) return;
        const int mw = effMinW(w);
        const bool bottleneck = mw >= threshold
            && (w->layout() == nullptr
                || w->minimumWidth() > 0
                || w->maximumWidth() < QWIDGETSIZE_MAX
                || qobject_cast<QLabel*>(w));
        if (bottleneck) {
            rows.append({mw, path + w->metaObject()->className()
                                + QLatin1Char('[') + w->objectName() + QLatin1Char(']')});
            return;
        }
        const QString childPath = path + w->metaObject()->className()
                                + QLatin1Char('[') + w->objectName() + QStringLiteral("]> ");
        for (auto* c : w->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            walk(c, childPath);
    };
    walk(&root, QString());
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.w > b.w; });
    QJsonArray out;
    int n = 0;
    for (const auto& r : rows) {
        if (n++ >= limit) break;
        out.append(QJsonObject{{"minW", r.w}, {"path", r.path}});
    }
    return out;
}

// Vertical counterpart of wideLeaves: the leaves whose minimum height (or
// height-for-width hint) hardens the host's vertical minimum.
QJsonArray tallLeaves(QWidget& root, int threshold, int limit = 12)
{
    struct Row { int h; QString path; };
    QList<Row> rows;
    std::function<void(QWidget*, const QString&)> walk =
        [&](QWidget* w, const QString& path) {
        if (!w) return;
        const int mh = effMinH(w);
        const bool bottleneck = mh >= threshold
            && (w->layout() == nullptr
                || w->minimumHeight() > 0
                || w->maximumHeight() < QWIDGETSIZE_MAX
                || qobject_cast<QLabel*>(w)
                || qobject_cast<QAbstractButton*>(w)
                || qobject_cast<QAbstractScrollArea*>(w));
        if (bottleneck) {
            rows.append({mh, path + w->metaObject()->className()
                                + QLatin1Char('[') + w->objectName() + QLatin1Char(']')});
            return;
        }
        const QString childPath = path + w->metaObject()->className()
                                + QLatin1Char('[') + w->objectName() + QStringLiteral("]> ");
        for (auto* c : w->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            walk(c, childPath);
    };
    walk(&root, QString());
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.h > b.h; });
    QJsonArray out;
    int n = 0;
    for (const auto& r : rows) {
        if (n++ >= limit) break;
        out.append(QJsonObject{{"minH", r.h}, {"path", r.path}});
    }
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    QString out, tag;
    int width = 1366, height = 768;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        auto next = [&]() { return QString::fromLocal8Bit(argv[++i]); };
        if (a == "--out") out = next();
        else if (a == "--tag") tag = next();
        else if (a == "--width") width = next().toInt();
        else if (a == "--height") height = next().toInt();
    }
    if (out.isEmpty()) return 3;
    QDir().mkpath(out);
    QDir().mkpath(out + "/profile");

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/consola.ttf");
    QFont::insertSubstitution("Manrope", "Segoe UI");
    QFont::insertSubstitution("JetBrains Mono", "Consolas");
    app.setFont(QFont("Segoe UI", 9));
    QCoreApplication::setOrganizationName("GlyphUiSweep");
    QCoreApplication::setApplicationName("MinW-" + tag);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, out + "/profile");

    MainWindow win(Bootstrapper::createContext());
    win.resize(width, height);
    win.show();
    drain(400);

    // Synthetic two-page fixture via QPdfWriter (same approach as the sweep probe).
    const QString basicPdf = out + "/fixture-basic.pdf";
    {
        QPdfWriter w(basicPdf);
        w.setPageSize(QPageSize(QPageSize::A4));
        w.setResolution(150);
        QPainter p(&w);
        p.setFont(QFont("Arial", 18));
        p.drawText(QPoint(200, 260), "minwidth fixture page one");
        w.newPage();
        p.drawText(QPoint(200, 260), "minwidth fixture page two");
        p.end();
    }
    win.openDocument(basicPdf);
    drain(700);

    QJsonObject audit;
    audit["tag"] = tag;
    audit["requested"] = QJsonObject{{"w", width}, {"h", height}};

    auto* ribbon = win.findChild<gp::Ribbon*>();
    auto* strip = win.findChild<gp::ModeStrip*>();
    auto* left = win.findChild<gp::Sidebar*>(QStringLiteral("leftSidebar"));
    auto* right = win.findChild<gp::Sidebar*>(QStringLiteral("rightSidebar"));
    auto* modes = win.findChild<gp::ModeController*>();
    auto* nav = win.findChild<gp::ScreenNav*>();
    auto* status = win.findChild<gp::StatusBar*>();

    auto regionRow = [&](const char* key, QWidget* w) {
        return QJsonObject{{"region", key}, {"minW", effMinW(w)}, {"currentW", w ? w->width() : -1},
                           {"minH", effMinH(w)}, {"currentH", w ? w->height() : -1}};
    };
    audit["chrome"] = QJsonArray{regionRow("ribbon", ribbon), regionRow("modeStrip", strip),
                                 regionRow("left", left), regionRow("right", right),
                                 regionRow("modes", modes), regionRow("screenNav", nav),
                                 regionRow("status", status)};
    audit["windowMinW"] = win.minimumSizeHint().width();
    audit["windowMinH"] = win.minimumSizeHint().height();

    // The right-slot widget is whichever visible row child sits AFTER the
    // center stack (Sidebar, SignaturesPanel, MeasureMode, A11y panel, ...).
    auto rightSlot = [&]() -> QWidget* {
        if (!modes) return right;
        QWidget* row = modes->parentWidget();
        auto* rowLay = qobject_cast<QHBoxLayout*>(row->layout());
        if (!rowLay) return right;
        const int centerIdx = rowLay->indexOf(modes);
        for (int i = centerIdx + 1; i < rowLay->count(); ++i) {
            QWidget* w = rowLay->itemAt(i)->widget();
            if (w && w->isVisible()) return w;
        }
        return nullptr;
    };

    QJsonArray screens;
    const QStringList ids = {"viewer", "compare", "batch", "ocr", "redact", "signature",
                             "measure", "accessibility", "form", "pdfa", "pages"};
    for (const QString& id : ids) {
        if (id != "viewer") {
            win.activateScreen(id);
            drain(350);
        }
        QWidget* cur = modes ? modes->currentWidget() : nullptr;
        QWidget* rp = rightSlot();
        const int rowMin = effMinW(left) + effMinW(cur) + effMinW(rp) + 0;
        QJsonObject s{{"id", id}};
        s["windowMinW"] = win.minimumSizeHint().width();   // max-so-far (stack cache)
        s["windowMinH"] = win.minimumSizeHint().height();
        s["windowActualW"] = win.width();
        s["windowActualH"] = win.height();
        s["rowMinFresh"] = rowMin;                          // left + current center + right slot
        if (modes) {
            QJsonArray pages;
            for (int i = 0; i < modes->count(); ++i) {
                QWidget* pg = modes->widget(i);
                if (!pg) continue;
                pages.append(QJsonObject{{"class", pg->metaObject()->className()},
                                         {"minW", effMinW(pg)},
                                         {"minH", effMinH(pg)},
                                         {"hidden", pg->isHidden()},
                                         {"visible", pg->isVisible()}});
            }
            s["stackPages"] = pages;
        }
        s["center"] = QJsonObject{{"class", cur ? cur->metaObject()->className() : "?"},
                                  {"minW", effMinW(cur)},
                                  {"minH", effMinH(cur)},
                                  {"leaves", cur ? wideLeaves(*cur, 100) : QJsonArray()},
                                  {"tall", cur ? tallLeaves(*cur, 80) : QJsonArray()}};
        s["rightPanel"] = QJsonObject{{"class", rp ? rp->metaObject()->className() : "(none)"},
                                      {"name", rp ? rp->objectName() : QString()},
                                      {"minW", effMinW(rp)},
                                      {"minH", effMinH(rp)},
                                      {"leaves", rp ? wideLeaves(*rp, 100) : QJsonArray()}};
        s["chromeAt"] = QJsonArray{regionRow("ribbon", ribbon), regionRow("modeStrip", strip),
                                   regionRow("screenNav", nav), regionRow("status", status)};
        screens.append(s);
    }
    audit["screens"] = screens;

    QFile f(out + "/minwidth-" + tag + ".json");
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(audit).toJson());
    QFile rc(out + "/rc-" + tag + ".txt");
    if (rc.open(QIODevice::WriteOnly))
        rc.write("DONE\n");
    return 0;
}
