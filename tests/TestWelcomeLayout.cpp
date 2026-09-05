// SPDX-License-Identifier: Apache-2.0
// U01 — welcome layout regression coverage (GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN,
// §5 "U01"). Layout-only packages may rely on layout inspection, but the plan
// requires automated coverage for real overflow/focus regressions. These tests
// pin exactly those, using only pre-existing widget API so they also compile
// against (and fail on) the pre-U01 implementation:
//   (a) no action card overflows its clipping bounds at 1280x720 — the old
//       six-card row needed 6*140+5*12 = 900px inside a 600px container, so the
//       final cards were clipped despite a large window;
//   (b) the responsive grid reflows to fewer columns as the widget narrows
//       (3 -> 2 -> 1 cards per row) and back.
// Purely visual spacing/palette/icon artwork remains a manual layout-inspection
// concern (plan §U01 acceptance) and is deliberately not pixel-pinned here.
#include <QtTest/QtTest>
#include <QApplication>
#include <QHash>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>

#include "ui/WelcomeWidget.h"

class TestWelcomeLayout : public QObject {
    Q_OBJECT

    static QList<QPushButton*> cards(const WelcomeWidget& w)
    {
        return w.findChildren<QPushButton*>(QStringLiteral("actionCard"));
    }

    static void flushLayout()
    {
        QApplication::processEvents();
        QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QApplication::processEvents();
    }

    // Max number of cards sharing one row, measured from live geometry.
    static int cardsPerRow(const QList<QPushButton*>& cardList, QWidget& origin)
    {
        QHash<int, int> rows; // mapped top-y -> card count
        for (const auto* card : cardList) {
            const QPoint topLeft =
                const_cast<QPushButton*>(card)->mapTo(&origin, card->rect().topLeft());
            ++rows[topLeft.y()];
        }
        int perRow = 0;
        for (auto it = rows.constBegin(); it != rows.constEnd(); ++it)
            perRow = qMax(perRow, it.value());
        return perRow;
    }

private slots:
    void initTestCase()
    {
        // Isolate QSettings: never read the user's real prefs, never clobber
        // them (same idiom as TestOcrPreprocessPrefs).
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestWelcomeLayout"));
    }

    // (a) Geometric overflow check: after a resize + layout flush, every
    // action card must fit inside the widget that clips it, at each of the
    // plan §U01 acceptance sizes. The old row laid the last cards out past
    // the 600px column's right edge.
    void noCardClipsAt1280x720()
    {
        const QList<QSize> sizes = {QSize(1280, 720), QSize(1440, 900), QSize(2560, 1440)};
        for (const QSize& size : sizes) {
            WelcomeWidget w;
            w.resize(size);
            w.show();
            flushLayout();

            const auto cardList = cards(w);
            QCOMPARE(cardList.size(), 6);

            auto* scroll = w.findChild<QScrollArea*>();
            QVERIFY(scroll);
            QWidget* content = scroll->widget();
            QVERIFY(content);

            for (const auto* card : cardList) {
                const QString where = QStringLiteral("%1x%2 %3")
                                          .arg(size.width())
                                          .arg(size.height())
                                          .arg(card->accessibleName());
                QVERIFY2(card->width() <= scroll->viewport()->width(),
                         qPrintable(QStringLiteral("%1: card width %2px exceeds the viewport %3px")
                                        .arg(where)
                                        .arg(card->width())
                                        .arg(scroll->viewport()->width())));
                // A child widget is clipped to its parent: a card whose right
                // edge lies past the parent's width is cut off on screen.
                QWidget* clipper = card->parentWidget();
                QVERIFY2(clipper != nullptr, "action card must have a clipping parent");
                QVERIFY2(card->geometry().right() <= clipper->width(),
                         qPrintable(QStringLiteral(
                                        "%1: card right edge %2px overflows its %3px-wide "
                                        "container (clipped)")
                                        .arg(where)
                                        .arg(card->geometry().right())
                                        .arg(clipper->width())));
                const QPoint topLeft =
                    const_cast<QPushButton*>(card)->mapTo(content, card->rect().topLeft());
                QVERIFY2(topLeft.x() >= 0 && topLeft.x() + card->width() <= content->width(),
                         qPrintable(QStringLiteral("%1: card spans x=[%2..%3] of %4px content width")
                                        .arg(where)
                                        .arg(topLeft.x())
                                        .arg(topLeft.x() + card->width())
                                        .arg(content->width())));
            }
        }
    }

    // (b) Responsive reflow: fewer columns as the widget narrows, more as it
    // widens. Driven by the widget's available width (never a display-size
    // assumption), so the same widget must change column counts on resize.
    void gridReflowsToFewerColumnsWhenNarrowed()
    {
        WelcomeWidget w;
        w.resize(1280, 720);
        w.show();
        flushLayout();

        const auto cardList = cards(w);
        QCOMPARE(cardList.size(), 6);
        QCOMPARE(cardsPerRow(cardList, w), 3);

        w.resize(400, 720);
        flushLayout();
        QCOMPARE(cardsPerRow(cardList, w), 2);

        w.resize(260, 720);
        flushLayout();
        QCOMPARE(cardsPerRow(cardList, w), 1);

        // Growing back must restore the roomy layout (no one-way latch).
        w.resize(1280, 720);
        flushLayout();
        QCOMPARE(cardsPerRow(cardList, w), 3);
    }

    // Supporting pins (pass before and after; guard the redesign against
    // regressions): Open PDF first, keyboard-named cards, the §9.16
    // local-processing tooltips, and recent files below the actions.
    void openPdfFirstAccessibleCardsTooltipsAndRecentsBeneath()
    {
        WelcomeWidget w;
        w.setRecentFiles({QStringLiteral("C:/__glyphpdf_u01_test_missing.pdf")});
        w.resize(1280, 720);
        w.show();
        flushLayout();

        const auto cardList = cards(w);
        QCOMPARE(cardList.size(), 6);

        // Creation (tab) order starts with the primary action.
        QCOMPARE(cardList.first()->accessibleName(), QStringLiteral("Open PDF file"));
        for (const auto* card : cardList) {
            QVERIFY2(!card->accessibleName().isEmpty(),
                     "every action card needs a keyboard/AT-visible name");
            QVERIFY2(!card->accessibleDescription().isEmpty(),
                     "every action card needs an accessible description");
        }

        // §9.16: exactly the Import Office / Images to PDF cards carry the
        // local-processing notice as their tooltip.
        for (const auto* card : cardList) {
            const bool isLocalNoticeCard =
                card->accessibleName().contains(QStringLiteral("Import Office")) ||
                card->accessibleName() == QStringLiteral("Images to PDF");
            QCOMPARE(!card->toolTip().isEmpty(), isLocalNoticeCard);
            if (isLocalNoticeCard)
                QVERIFY2(card->toolTip().contains(QStringLiteral("locally")),
                         "the notice must keep saying the processing is local");
        }

        // Recent files sit directly beneath the action cards (never above or
        // pushed to a distant region of the page).
        auto* content = w.findChild<QScrollArea*>()->widget();
        QVERIFY(content);
        int maxCardBottom = 0;
        for (const auto* card : cardList) {
            const QPoint topLeft = const_cast<QPushButton*>(card)->mapTo(content, card->rect().topLeft());
            maxCardBottom = qMax(maxCardBottom, topLeft.y() + card->height());
        }
        auto* recentList = w.findChild<QListWidget*>(QStringLiteral("recentFilesList"));
        QVERIFY(recentList);
        QVERIFY(recentList->isVisible());
        const int recentTop =
            recentList->mapTo(content, recentList->rect().topLeft()).y();
        QVERIFY2(recentTop >= maxCardBottom,
                 qPrintable(QStringLiteral("recent list top %1 must be at or below the "
                                          "lowest card bottom %2")
                                .arg(recentTop)
                                .arg(maxCardBottom)));
    }
};

QTEST_MAIN(TestWelcomeLayout)
#include "TestWelcomeLayout.moc"
