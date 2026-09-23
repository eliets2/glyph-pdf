// SPDX-License-Identifier: Apache-2.0
// SWEEP-W3-UI offscreen acceptance-evidence probe (ui-specialist lane,
// 2026-09-20, tip b17106a).
//
// One process renders ONE cell of the R17 matrix (a window size x a
// QT_SCALE_FACTOR, both supplied by the runner) and captures, via
// QWidget::grab (no native desktop, QT_QPA_PLATFORM=offscreen):
//   welcome + ribbon tabs (post-open) + task screens + comments pane +
//   dialogs + themes + CJK fixture, each as PNG plus a machine-audit JSON
//   (texts, enabled states, icon-asset existence, tooltips, clip/truncation
//   suspects, focus chains, status-bar disclosures).
//
// Harness adaptations (disclosed, same class as UI-BUTTON-REVIEW-2026-09-09):
//   - offscreen has no desktop font fallback: Segoe UI/Consolas are loaded
//     explicitly and Manrope/JetBrains Mono substituted (the app bundles
//     them only when resources/fonts/ exists — main.cpp tolerates absence;
//     the deployed app resolves system fonts on Windows).
//   - resources.qrc (icons + theme QSS) belongs to the PdfWorkstation exe;
//     the build script compiles the SAME qrc into this probe so icons and
//     QSS render exactly as shipped (circle-fallback would be a harness
//     artifact, not product evidence).
//   - dialogs are shown with show() (non-modal) so automation can grab and
//     close them deterministically; the production exec() entry points are
//     exercised by the R15-R17 test waves (documented, not re-litigated).
// Production code is NOT modified by this probe. All writes go to --out.
//
// stdout is unreliable under bash -lc on this machine: results go to
// <out>/audit-<tag>.json and <out>/rc-<tag>.txt ("DONE" = finished).

#include <QApplication>
#include <QCheckBox>
#include <cmath>
#include <QStringList>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFontInfo>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QStyleFactory>
#include <QPageSize>
#include <QPdfWriter>
#include <QTimer>
#include <QToolButton>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/ToolId.h"
#include "engines/SignatureManager.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "modes/BatchMode.h"
#include "modes/ModeController.h"
#include "modes/RedactApplyDialog.h"
#include "shell/Ribbon.h"
#include "shell/RibbonModel.h"
#include "shell/StatusBar.h"
#include "shell/ToolRegistry.h"
#include "ui/OcspConsentDialog.h"
#include "ui/PreferencesDialog.h"
#include "ui/RecipientPickerDialog.h"
#include "ui/SignatureDialog.h"
#include "ui/PdfViewerWidget.h"
#include "ui/SigningProgressPanel.h"
#include "ui/SigningRequestDialog.h"
#include "ui/WelcomeWidget.h"
#include "util/GpTheme.h"
#include "util/Icons.h"

using gp::MainWindow;

namespace {

void drain(int ms = 250)
{
    QEventLoop e;
    QTimer::singleShot(ms, &e, &QEventLoop::quit);
    e.exec();
}

QSize grabPng(QWidget& w, const QString& path)
{
    QPixmap pm = w.grab();
    pm.save(path, "PNG");
    return pm.size();
}

QJsonObject sizeJson(const QSize& s)
{
    return QJsonObject{{"w", s.width()}, {"h", s.height()}};
}

double contrastRatio(const QColor& a, const QColor& b)
{
    auto lum = [](const QColor& c) {
        auto ch = [](int v) {
            const double s = v / 255.0;
            return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * ch(c.red()) + 0.7152 * ch(c.green()) + 0.0722 * ch(c.blue());
    };
    const double l1 = lum(a), l2 = lum(b);
    const double hi = qMax(l1, l2), lo = qMin(l1, l2);
    return (hi + 0.05) / (lo + 0.05);
}

// Truncation / clipping suspects. Soft findings for triage, never auto-fail:
// wrapped labels and scroll-area children are legitimately larger than their
// allocated rect.
namespace {

bool insideScrollArea(QWidget* w)
{
    for (QWidget* p = w->parentWidget(); p; p = p->parentWidget())
        if (qobject_cast<QScrollArea*>(p))
            return true;
    return false;
}

} // namespace

namespace {

QJsonArray clipSuspects(QWidget& root)
{
    QJsonArray out;
    for (auto* lbl : root.findChildren<QLabel*>()) {
        if (!lbl->isVisible() || lbl->wordWrap() || lbl->text().isEmpty() || lbl->width() <= 0)
            continue;
        const int need = lbl->fontMetrics().horizontalAdvance(lbl->text());
        if (need > lbl->width() + 1 && !insideScrollArea(lbl)) {
            out.append(QJsonObject{{"class", "QLabel"},
                                   {"text", lbl->text().left(60)},
                                   {"objectName", lbl->objectName()},
                                   {"needPx", need},
                                   {"havePx", lbl->width()}});
        }
    }
    return out;
}

} // namespace

QJsonArray focusChain(QWidget& start, int hops)
{
    QJsonArray out;
    QWidget* w = &start;
    for (int i = 0; i < hops && w; ++i) {
        w = w->nextInFocusChain();
        if (!w || w == &start)
            break;
        out.append(QJsonObject{{"class", w->metaObject()->className()},
                               {"name", w->objectName()},
                               {"text", w->property("text").toString().left(40)}});
    }
    return out;
}

QJsonObject widgetCounts(QWidget& root)
{
    QJsonObject o;
    int btns = 0, blankBtns = 0, noTip = 0;
    for (auto* b : root.findChildren<QToolButton*>()) {
        if (!b->isVisible())
            continue;
        ++btns;
        if (b->text().simplified().isEmpty() && !b->defaultAction())
            ++blankBtns;
        if (b->toolTip().simplified().isEmpty() && b->statusTip().simplified().isEmpty())
            ++noTip;
    }
    o["visibleToolButtons"] = btns;
    o["blankTiles"] = blankBtns;
    o["noTipButtons"] = noTip;
    return o;
}

QJsonObject statusBarState(MainWindow& win)
{
    QJsonObject o;
    if (auto* sb = win.findChild<gp::StatusBar*>()) {
        QJsonArray labels;
        for (auto* l : sb->findChildren<QLabel*>())
            if (l->isVisible() && !l->text().simplified().isEmpty())
                labels.append(l->text());
        o["labels"] = labels;
        o["currentMessage"] = sb->currentMessage();
    } else {
        o["found"] = false;
    }
    return o;
}

QJsonObject dialogAudit(QDialog& d)
{
    QJsonObject o;
    o["title"] = d.windowTitle();
    o["modality"] = (d.windowModality() == Qt::NonModal ? QString("nonModal(probe show())") : QString("modal"));
    QJsonArray btns, checks, combos, radios, labels;
    for (auto* b : d.findChildren<QPushButton*>())
        if (b->isVisible())
            btns.append(b->text().simplified());
    for (auto* c : d.findChildren<QCheckBox*>())
        if (c->isVisible())
            checks.append(QJsonObject{{"text", c->text().simplified()}, {"checked", c->isChecked()}, {"enabled", c->isEnabled()}});
    for (auto* r : d.findChildren<QRadioButton*>())
        if (r->isVisible())
            radios.append(r->text().simplified());
    for (auto* c : d.findChildren<QComboBox*>())
        if (c->isVisible())
            combos.append(QJsonObject{{"current", c->currentText()},
                                      {"items", QJsonArray::fromStringList([&] {
                                           QStringList items;
                                           for (int i = 0; i < c->count(); ++i) items << c->itemText(i);
                                           return items;
                                       }())}});
    for (auto* l : d.findChildren<QLabel*>())
        if (l->isVisible() && !l->text().simplified().isEmpty())
            labels.append(l->text().simplified().left(120));
    o["buttons"] = btns;
    o["checkboxes"] = checks;
    o["combos"] = combos;
    o["radios"] = radios;
    o["labels"] = labels;
    o["focusChain"] = focusChain(d, 12);
    o["clipSuspects"] = clipSuspects(d);
    return o;
}

void makeTextPdf(const QString& path, const QList<QPair<QString, QString>>& lines)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    w.setResolution(150);
    QPainter p(&w);
    int y = 260;
    for (const auto& line : lines) {
        p.setFont(QFont(line.second, 18));
        p.drawText(QPoint(200, y), line.first);
        y += 330;
    }
    p.end();
}

// First installed family from `candidates` (exact, case-insensitive), or empty.
QString firstInstalledFont(const QStringList& candidates)
{
    const QStringList families = QFontDatabase::families();
    for (const QString& want : candidates)
        for (const QString& fam : families)
            if (fam.compare(want, Qt::CaseInsensitive) == 0)
                return fam;
    return QString();
}

} // namespace

int main(int argc, char** argv)
{
    QString out, tag, fixturesDir;
    int width = 1366, height = 768;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        auto next = [&]() { return QString::fromLocal8Bit(argv[++i]); };
        if (a == "--out") out = next();
        else if (a == "--tag") tag = next();
        else if (a == "--width") width = next().toInt();
        else if (a == "--height") height = next().toInt();
        else if (a == "--fixtures") fixturesDir = next();
    }
    if (out.isEmpty())
        return 3;
    QDir().mkpath(out);
    QDir().mkpath(out + "/profile");
    QDir().mkpath(out + "/presets");

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setStyle(QStyleFactory::create("Fusion"));

    // Harness adaptation: offscreen font fallback (disclosed in header).
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/consola.ttf");
    QFont::insertSubstitution("Manrope", "Segoe UI");
    QFont::insertSubstitution("JetBrains Mono", "Consolas");
    app.setFont(QFont("Segoe UI", 9));

    QCoreApplication::setOrganizationName("GlyphUiSweep");
    QCoreApplication::setApplicationName("W3-" + tag);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, out + "/profile");

    QJsonObject audit;
    audit["tag"] = tag;
    audit["requestedSize"] = QJsonObject{{"w", width}, {"h", height}};
    audit["qtScaleFactorEnv"] = qEnvironmentVariable("QT_SCALE_FACTOR");
    audit["qrcIcons"] = QFile::exists(":/resources/icons/save.svg") ? QString("ok") : QString("MISSING(harness)");

    // Fixtures (synthetic; no private content).
    const QString basicPdf = out + "/fixture-basic.pdf";
    makeTextPdf(basicPdf, {{"GlyphPDF W3 UI sweep", "Arial"},
                           {"Synthetic fixture - no private content", "Arial"},
                           {"Page one of a two-page proof.", "Arial"},
                           {"Second page filler text.", "Arial"}});
    const QString cjkPdf = out + "/fixture-cjk.pdf";
    QJsonArray cjkLineFonts;
    {
        // Per-script font selection. A line is SKIPPED (and recorded) when the
        // host has no family covering it — embedding glyphs from a
        // non-covering font would bake tofu into the fixture itself and
        // invalidate the no-tofu claim. This host (Windows, en locale) has
        // Malgun Gothic (ko + ja kana/kanji + CJK ideographs) but NO
        // simplified-Chinese family (only SimSun-ExtB, a rare-ideograph
        // supplement) — zh-CN is recorded as host-limited, not silently
        // dropped.
        const QString koFont = firstInstalledFont({"Malgun Gothic", "Malgun Gothic UI", "Gulim"});
        // Malgun Gothic covers kana + the common kanji as a fallback on hosts
        // without a dedicated ja family (verified rendering ko/ja lines).
        const QString jaFont = firstInstalledFont({"Yu Gothic UI", "Yu Gothic", "Meiryo", "MS Gothic", "Malgun Gothic"});
        const QString zhFont = firstInstalledFont({"Microsoft YaHei UI", "Microsoft YaHei", "SimSun",
                                                   "NSimSun", "DengXian", "Noto Sans CJK SC", "Source Han Sans SC"});
        const QString arFont = firstInstalledFont({"Segoe UI", "Tahoma", "Arial"});
        struct CjkLine { const char* text; QString font; };
        const QList<QPair<QString, QString>> cjkLines = {
            {QString::fromUtf8("GlyphPDF CJK / RTL smoke:"), arFont},
            {QString::fromUtf8("RTL: \xD8\xA7\xD8\xAE\xD8\xAA\xD8\xA8\xD8\xA7\xD8\xB1 \xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9"), arFont},
            {QString::fromUtf8("\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4 \xED\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8 (ko)"), koFont},
            {QString::fromUtf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88 (ja)"), jaFont},
            {QString::fromUtf8("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95 (zh)"), zhFont},
        };
        QList<QPair<QString, QString>> embedded;
        for (const auto& line : cjkLines) {
            if (line.second.isEmpty()) {
                cjkLineFonts.append(QJsonObject{{"text", line.first}, {"embedded", false}});
            } else {
                embedded.append(line);
                cjkLineFonts.append(QJsonObject{{"text", line.first}, {"embedded", true}, {"font", line.second}});
            }
        }
        audit["cjkLineFonts"] = cjkLineFonts;
        audit["cjkFixtureFamily"] = koFont;
        makeTextPdf(cjkPdf, embedded);
    }

    MainWindow win(Bootstrapper::createContext());
    win.resize(width, height);
    win.show();
    drain(400);
    audit["actualSize"] = QJsonObject{{"w", win.width()}, {"h", win.height()}};
    audit["devicePixelRatio"] = win.devicePixelRatioF();

    // ── Cell 1: welcome ──────────────────────────────────────────────────────
    {
        QJsonObject o;
        o["screenshot"] = "welcome.png";
        grabPng(win, out + "/welcome.png");
        if (auto* ww = win.findChild<WelcomeWidget*>()) {
            o["found"] = true;
            QJsonArray cards, links;
            int visibleCards = 0;
            for (auto* b : ww->findChildren<QPushButton*>()) {
                if (b->objectName().startsWith("welcomeCard-")) {
                    cards.append(QJsonObject{{"id", b->objectName()},
                                             {"text", b->text().simplified()},
                                             {"accessibleName", b->accessibleName()},
                                             {"enabled", b->isEnabled()},
                                             {"visible", b->isVisible()}});
                    if (b->isVisible())
                        ++visibleCards;
                } else if (b->objectName().startsWith("welcomeLink-")) {
                    links.append(QJsonObject{{"id", b->objectName()}, {"text", b->text().simplified()}, {"visible", b->isVisible()}});
                }
            }
            o["cards"] = cards;
            o["visibleCardCount"] = visibleCards;
            o["links"] = links;
            o["dropTargetTexts"] = QJsonArray();
            QJsonArray drop;
            for (auto* l : ww->findChildren<QLabel*>())
                if (l->isVisible() && (l->text().contains("drop", Qt::CaseInsensitive) || l->text().contains("drag", Qt::CaseInsensitive)))
                    drop.append(l->text().simplified().left(80));
            o["dropTargetTexts"] = drop;
            o["welcomeAcceptsDrops"] = ww->acceptDrops();
            o["clipSuspects"] = clipSuspects(*ww);
        } else {
            o["found"] = false;
        }
        audit["welcome"] = o;
    }

    // Open a document for the workspace stages.
    win.openDocument(basicPdf);
    drain(700);
    audit["viewerPages"] = win.pdfViewer() ? win.pdfViewer()->pageCount() : -1;

    // ── Cell 2: ribbon tabs ──────────────────────────────────────────────────
    {
        QJsonObject o;
        auto* ribbon = win.findChild<gp::Ribbon*>();
        auto* reg = win.findChild<gp::ToolRegistry*>();
        o["found"] = ribbon != nullptr;
        if (ribbon) {
            ribbon->setCollapsed(false);
            drain();
            QJsonArray tabsAudit;
            for (const auto& tab : gp::RibbonModel::tabs()) {
                ribbon->raiseTab(tab.name);
                drain();
                grabPng(win, out + "/tab-" + tab.name.toLower() + ".png");
                QJsonObject t{{"tab", tab.name}, {"screenshot", "tab-" + tab.name.toLower() + ".png"}};
                QJsonArray entries;
                int rendered = 0, blank = 0, noTip = 0, missingAsset = 0, stateMismatch = 0;
                for (const auto& group : tab.groups) {
                    for (const auto& tool : group.tools) {
                        const bool planned = gp::RibbonModel::plannedTools().contains(tool.id);
                        QToolButton* button = nullptr;
                        for (auto* b : ribbon->findChildren<QToolButton*>())
                            if (b->property("toolId").toString() == tool.id)
                                button = b;
                        QJsonObject e{{"id", tool.id},
                                      {"label", tool.label},
                                      {"plannedHidden", planned},
                                      {"buttonVisible", button && button->isVisible()}};
                        if (button && button->isVisible()) {
                            ++rendered;
                            const bool blankTile = button->text().simplified().isEmpty();
                            e["buttonText"] = button->text().simplified();
                            e["blankTile"] = blankTile;
                            if (blankTile)
                                ++blank;
                            e["hasToolTip"] = !button->toolTip().simplified().isEmpty();
                            if (!e["hasToolTip"].toBool())
                                ++noTip;
                            const bool asset = QFile::exists(":/resources/icons/" + tool.icon + ".svg");
                            e["iconAssetExists"] = asset;
                            if (!asset)
                                ++missingAsset;
                            e["buttonEnabled"] = button->isEnabled();
                            if (!button->isEnabled()) {
                                // R15/R17 honesty: a disabled control discloses
                                // its reason on accessible channels, never
                                // tooltip-only.
                                e["disabledStatusTip"] = button->statusTip().simplified().left(120);
                                e["disabledAccessibleDescription"] = button->accessibleDescription().simplified().left(120);
                                e["disabledToolTip"] = button->toolTip().simplified().left(120);
                            }
                            const auto id = toolIdFromString(tool.id);
                            e["hasController"] = id && reg && reg->controllerFor(*id) != nullptr;
                            if (id && reg && reg->controllerFor(*id)) {
                                const bool actEn = reg->actionFor(*id)->isEnabled();
                                e["actionEnabled"] = actEn;
                                if (actEn != button->isEnabled())
                                    ++stateMismatch;
                            }
                        }
                        entries.append(e);
                    }
                }
                t["rendered"] = rendered;
                t["blankTiles"] = blank;
                t["noToolTip"] = noTip;
                t["missingIconAsset"] = missingAsset;
                t["buttonVsActionStateMismatch"] = stateMismatch;
                t["entries"] = entries;
                tabsAudit.append(t);
            }
            o["tabs"] = tabsAudit;
        }
        audit["ribbon"] = o;
    }

    // ── Cell 3: task screens ─────────────────────────────────────────────────
    {
        QJsonObject o;
        auto* modes = win.findChild<gp::ModeController*>();
        o["modeControllerFound"] = modes != nullptr;
        // Panel/widget screens: activateScreen swaps or raises the surface.
        const QStringList ids = {"compare", "batch", "ocr", "redact", "signature",
                                 "measure", "accessibility", "form", "pdfa", "pages"};
        QJsonArray screens;
        for (const QString& id : ids) {
            win.activateScreen(id);
            drain(400);
            QJsonObject s{{"id", id},
                          {"screenshot", "screen-" + id + ".png"}};
            const QSize grabbed = grabPng(win, out + "/screen-" + id + ".png");
            // Honesty per UI-BUTTON-REVIEW lesson: record the ACTUAL grab
            // size — a window that grew past the requested viewport is a
            // finding (minimum-size policy vs the acceptance resolution),
            // never a silent pass.
            s["grabSize"] = QJsonObject{{"w", grabbed.width()}, {"h", grabbed.height()}};
            s["grewPastRequested"] = grabbed.width() > width || grabbed.height() > height;
            s["currentScreen"] = modes ? modes->currentScreen() : QString("?");
            s["statusBar"] = statusBarState(win);
            s["controls"] = widgetCounts(win);
            s["clipSuspects"] = clipSuspects(win);
            screens.append(s);
        }
        // Compress / Watermark are MODAL task dialogs on the production path
        // (GpMainWindow onToolActivated -> CompressDialog/WatermarkDialog
        // .exec()). Drive the same production entry and capture the ACTIVE
        // MODAL from a queued timer (modal exec() runs timers), then close it.
        for (const QString& id : {"compress", "watermark"}) {
            QTimer::singleShot(900, [&, id] {
                if (auto* m = QApplication::activeModalWidget()) {
                    m->grab().save(out + "/screen-" + id + "-dialog.png", "PNG");
                    m->close();
                }
            });
            win.activateScreen(id);
            drain(500);
            QJsonObject s{{"id", id}};
            const QString modalShot = out + "/screen-" + id + "-dialog.png";
            if (QFile::exists(modalShot)) {
                s["screenshot"] = "screen-" + id + "-dialog.png";
                s["modalOpened"] = true;
            } else {
                s["screenshot"] = "screen-" + id + ".png";
                s["modalOpened"] = false;
                grabPng(win, out + "/screen-" + id + ".png");
            }
            s["statusBar"] = statusBarState(win);
            screens.append(s);
        }
        // Batch: Preset Pipeline panel (op index 7, R26).
        gp::BatchMode::setPresetStoreDirForTest(out + "/presets");
        win.activateScreen("batch");
        drain(300);
        QJsonObject pp{{"screenshot", "screen-batch-preset-pipeline.png"}};
        if (auto* batch = win.findChild<gp::BatchMode*>()) {
            batch->setOperationForTest(7);
            drain(300);
            const QSize ppSize = grabPng(win, out + "/screen-batch-preset-pipeline.png");
            pp["grabSize"] = QJsonObject{{"w", ppSize.width()}, {"h", ppSize.height()}};
            pp["clipSuspects"] = clipSuspects(win);
            QJsonArray pipelineLabels;
            for (auto* l : batch->findChildren<QLabel*>())
                if (l->isVisible() && !l->text().simplified().isEmpty())
                    pipelineLabels.append(l->text().simplified().left(80));
            pp["visibleLabels"] = pipelineLabels;
        } else {
            pp["batchFound"] = false;
        }
        screens.append(pp);
        o["screens"] = screens;

        // Comments pane — print summary surface entry point.
        win.showSidebarPane("comments");
        drain(300);
        const QSize cmSize = grabPng(win, out + "/screen-comments.png");
        QJsonObject cm{{"screenshot", "screen-comments.png"},
                       {"grabSize", QJsonObject{{"w", cmSize.width()}, {"h", cmSize.height()}}}};
        QJsonArray summaryButtons;
        for (auto* b : win.findChildren<QPushButton*>())
            if (b->isVisible() && b->text().contains("Summary", Qt::CaseInsensitive))
                summaryButtons.append(QJsonObject{{"text", b->text().simplified()}, {"enabled", b->isEnabled()}});
        cm["summaryButtons"] = summaryButtons;
        o["comments"] = cm;
        win.showSidebarPane("pages");
        drain(200);
        audit["screens"] = o;
    }

    // ── Cell 4: dialogs ──────────────────────────────────────────────────────
    {
        QJsonObject o;
        {
            SignatureDialog d(&win);
            d.show();
            drain();
            QJsonObject s = dialogAudit(d);
            s["screenshot"] = "dialog-sign.png";
            s["grabSize"] = sizeJson(grabPng(d, out + "/dialog-sign.png"));
            // certify selector presence (N18): the purpose combo's ITEMS carry
            // "Certify" (current is "Approve signature" by default).
            bool certifyUi = false;
            for (auto* r : d.findChildren<QRadioButton*>())
                if (r->text().contains("Certify", Qt::CaseInsensitive))
                    certifyUi = true;
            for (auto* c : d.findChildren<QComboBox*>())
                for (int i = 0; i < c->count(); ++i)
                    if (c->itemText(i).contains("Certify", Qt::CaseInsensitive))
                        certifyUi = true;
            s["certifySelectorPresent"] = certifyUi;
            if (auto* pc = d.findChild<QComboBox*>("signaturePurposeCombo")) {
                s["certifyPurposeCombo"] = true;
                QStringList items;
                for (int i = 0; i < pc->count(); ++i) items << pc->itemText(i);
                s["certifyPurposeItems"] = QJsonArray::fromStringList(items);
                s["certifyPurposeDefault"] = pc->currentText();
            }
            o["sign"] = s;
            d.close();
        }
        {
            gp::OcspConsentDialog d(&win);
            d.show();
            drain();
            QJsonObject s = dialogAudit(d);
            s["screenshot"] = "dialog-ocsp.png";
            s["grabSize"] = sizeJson(grabPng(d, out + "/dialog-ocsp.png"));
            o["ocsp"] = s;
            d.close();
        }
        {
            gp::PreferencesDialog d(&win);
            d.show();
            drain();
            QJsonObject s = dialogAudit(d);
            s["screenshot"] = "dialog-preferences.png";
            s["grabSize"] = sizeJson(grabPng(d, out + "/dialog-preferences.png"));
            QJsonArray policyRows;
            for (auto* c : d.findChildren<QCheckBox*>())
                if (c->text().contains("policy", Qt::CaseInsensitive) || c->toolTip().contains("policy", Qt::CaseInsensitive))
                    policyRows.append(QJsonObject{{"text", c->text().simplified()}, {"enabled", c->isEnabled()}});
            s["policyMentioningRows"] = policyRows;
            o["preferences"] = s;
            d.close();
        }
        {
            RecipientPickerDialog d(&win);
            d.show();
            drain();
            QJsonObject s = dialogAudit(d);
            s["screenshot"] = "dialog-certencrypt.png";
            s["grabSize"] = sizeJson(grabPng(d, out + "/dialog-certencrypt.png"));
            o["certEncryptPicker"] = s;
            d.close();
        }
        {
            gp::RedactApplyPlan plan;
            plan.sourcePath = basicPdf;
            plan.destinationPath = out + "/redacted.pdf";
            plan.sanitizedDestinationPath = out + "/redacted_sanitized.pdf";
            plan.markCount = 1;
            plan.marksPerPage[0] = 1;
            plan.sanitize = true;
            gp::RedactApplyDialog d(plan);
            d.show();
            drain();
            QJsonObject s = dialogAudit(d);
            s["screenshot"] = "dialog-redactapply.png";
            s["grabSize"] = sizeJson(grabPng(d, out + "/dialog-redactapply.png"));
            QJsonArray checked;
            for (auto* c : d.findChildren<QCheckBox*>())
                if (c->isVisible())
                    checked.append(QJsonObject{{"text", c->text().simplified()}, {"checked", c->isChecked()}});
            s["checkboxStates"] = checked;
            o["redactApply"] = s;
            d.close();
        }
        // Signing request prepare + progress (needs the repo signing fixtures).
        const QString fixtureDoc = fixturesDir + "/test_input.pdf";
        o["signingFixturesPresent"] = QFileInfo::exists(fixtureDoc);
        if (QFileInfo::exists(fixtureDoc)) {
            const QString doc = out + "/sign-doc.pdf";
            QFile::remove(doc);
            QFile::copy(fixtureDoc, doc);
            SignatureManager mgr;
            {
                SigningRequestDialog d(&mgr, doc, &win);
                d.show();
                drain();
                QJsonObject s = dialogAudit(d);
                s["screenshot"] = "dialog-signrequest.png";
                s["grabSize"] = sizeJson(grabPng(d, out + "/dialog-signrequest.png"));
                o["signRequest"] = s;
                d.close();
            }
            {
                SigningProgressPanel d(&mgr, doc, &win);
                d.show();
                drain();
                QJsonObject s = dialogAudit(d);
                s["screenshot"] = "dialog-signprogress.png";
                s["grabSize"] = sizeJson(grabPng(d, out + "/dialog-signprogress.png"));
                o["signProgress"] = s;
                d.close();
            }
        }
        audit["dialogs"] = o;
    }

    // ── Cell 5: themes (welcome + compare per theme) ─────────────────────────
    {
        QJsonObject o;
        const QString startScreen = "compare";
        win.activateScreen(startScreen);
        drain(300);
        for (int i = 0; i < 3; ++i) {
            QMetaObject::invokeMethod(&win, "toggleTheme", Qt::DirectConnection);
            drain(300);
            const QString name = gp::Theme::current() == gp::Theme::Dark ? "dark"
                                 : gp::Theme::current() == gp::Theme::Light ? "light"
                                                                            : "highcontrast";
            QJsonObject t{{"theme", name}};
            const QPalette pal = win.palette();
            t["contrastWindowVsText"] = contrastRatio(pal.color(QPalette::Window), pal.color(QPalette::WindowText));
            t["screenshotMode"] = startScreen + ".png";
            t["modeGrabSize"] = sizeJson(grabPng(win, out + "/theme-" + name + "-" + startScreen + ".png"));
            win.showWelcome();
            drain(300);
            t["screenshotWelcome"] = "welcome.png";
            t["welcomeGrabSize"] = sizeJson(grabPng(win, out + "/theme-" + name + "-welcome.png"));
            o[name] = t;
            win.activateScreen(startScreen);
            drain(200);
        }
        audit["themes"] = o;
    }

    // ── Cell 6: CJK fixture in the viewer + the R17 font-resolution pin ─────
    {
        win.openDocument(cjkPdf);
        drain(800);
        // Bring the VIEWER to the center (panel-only screens host _viewer);
        // opening a document does not reset a previously activated screen.
        win.activateScreen("pdfa");
        drain(400);
        const QSize cjkGrab = grabPng(win, out + "/cjk-viewer.png");
        QJsonObject o{{"screenshot", "cjk-viewer.png"},
                      {"grabSize", sizeJson(cjkGrab)},
                      {"viewerPages", win.pdfViewer() ? win.pdfViewer()->pageCount() : -1}};
        // Engine-level no-tofu evidence: a DIRECT synchronous render through
        // the app's production renderer (PdfiumBackend::renderPage — the same
        // pdfium raster path the viewer paints; offscreen the widget canvas
        // and PdfViewerWidget::renderPage both come back unpainted/black, a
        // disclosed harness limit since UI-BUTTON-REVIEW-2026-09-09).
        {
            PdfiumBackend backend;
            if (backend.loadDocument(cjkPdf) && backend.pageCount() > 0) {
                const QImage page = backend.renderPage(0, 144);
                page.save(out + "/cjk-engine-render.png", "PNG");
                o["engineRender"] = "cjk-engine-render.png";
                o["engineRenderSize"] = QJsonObject{{"w", page.width()}, {"h", page.height()}};
                // Non-blank check on a WHITE page background: the fraction of
                // dark pixels (glyph ink). ~0 = blank render (its own finding);
                // a few percent = real glyph ink.
                qint64 ink = 0;
                const QImage gray = page.convertToFormat(QImage::Format_Grayscale8);
                for (int y = 0; y < gray.height(); ++y) {
                    const uchar* line = gray.constScanLine(y);
                    for (int x = 0; x < gray.width(); ++x)
                        if (line[x] < 128)
                            ++ink;
                }
                o["engineRenderInkFraction"] = double(ink) / double(gray.width() * gray.height());
            } else {
                o["engineRender"] = "FAILED TO LOAD";
            }
        }
        // R17 TestUiAccessibility parity: a CJK QLabel resolves a renderable
        // family (pin reproduced here so the screenshot cell carries the same
        // machine evidence as the test wave).
        QLabel cjkLabel(QString::fromUtf8("GlyphPDF\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95"));
        cjkLabel.ensurePolished();
        const QFontInfo fi(cjkLabel.font());
        o["uiLabelResolvedFamily"] = fi.family();
        o["uiLabelSizeHintNonEmpty"] = cjkLabel.sizeHint().width() > 0;
        audit["cjk"] = o;
    }

    QFile f(out + "/audit-" + tag + ".json");
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(audit).toJson());
    QFile rc(out + "/rc-" + tag + ".txt");
    if (rc.open(QIODevice::WriteOnly))
        rc.write("DONE\n");
    return 0;
}
