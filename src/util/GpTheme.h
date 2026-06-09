// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QColor>
#include <QString>
#include <QApplication>
#include <QPalette>

namespace gp {

// Design tokens — single source of truth. Match HTML CSS vars exactly.
struct Theme {
    enum Mode { Dark, Light, HighContrast };

    // Spacing / metrics
    static constexpr int MenuBarH    = 26;
    static constexpr int RibbonTabH  = 30;
    static constexpr int RibbonBodyH = 88;
    static constexpr int ModeStripH  = 30;
    static constexpr int ToolbarH    = 32;
    static constexpr int InfoStripH  = 22;
    static constexpr int StatusH     = 24;
    static constexpr int ScreenNavH  = 26;
    static constexpr int RowH        = 28;
    static constexpr int LeftPaneW   = 248;
    static constexpr int RightPaneW  = 288;
    static constexpr int AiPaneW     = 340;
    static constexpr int IconSm      = 14;
    static constexpr int IconMd      = 16;
    static constexpr int IconLg      = 22;

    static QColor bg0()         { return current() == HighContrast ? QColor("#000000") : current() == Dark ? QColor("#1a1b1e") : QColor("#bdb8ad"); }
    static QColor bg1()         { return current() == HighContrast ? QColor("#000000") : current() == Dark ? QColor("#1e1f22") : QColor("#e9e6dd"); }
    static QColor bg2()         { return current() == HighContrast ? QColor("#1a1a1a") : current() == Dark ? QColor("#2b2d30") : QColor("#d8d5cb"); }
    static QColor bg3()         { return current() == HighContrast ? QColor("#2a2a2a") : current() == Dark ? QColor("#34363b") : QColor("#cac6bb"); }
    static QColor bg4()         { return current() == HighContrast ? QColor("#3a3a3a") : current() == Dark ? QColor("#3d4046") : QColor("#b8b4a8"); }
    static QColor line()        { return current() == HighContrast ? QColor("#ffffff") : current() == Dark ? QColor("#393b40") : QColor("#b0aca0"); }
    static QColor lineStrong()  { return current() == HighContrast ? QColor("#ffffff") : current() == Dark ? QColor("#4a4d52") : QColor("#8a8678"); }
    static QColor fg0()         { return current() == HighContrast ? QColor("#ffffff") : current() == Dark ? QColor("#dfe1e5") : QColor("#1a1b1e"); }
    static QColor fg1()         { return current() == HighContrast ? QColor("#ffffff") : current() == Dark ? QColor("#a8abb0") : QColor("#454650"); }
    static QColor fg2()         { return current() == HighContrast ? QColor("#e0e0e0") : current() == Dark ? QColor("#71747a") : QColor("#6e6f78"); }
    static QColor fg3()         { return current() == HighContrast ? QColor("#c0c0c0") : current() == Dark ? QColor("#52555a") : QColor("#9c9a90"); }
    static QColor accent()      { return current() == HighContrast ? QColor("#ffff00") : current() == Dark ? QColor("#ff8c42") : QColor("#c25a18"); }
    static QColor accentDim()   { auto c = accent(); c.setAlpha(0x22); return c; }
    static QColor highlight()   { return QColor("#f4d03f"); }
    static QColor note()        { return QColor("#ff6b8a"); }
    static QColor underline()   { return QColor("#4ec9b0"); }
    static QColor strike()      { return QColor("#c586c0"); }
    static QColor danger()      { return QColor("#c8442b"); }
    static QColor warning()     { return QColor("#F59E0B"); }
    static QColor info()        { return QColor("#7fb1e8"); }
    static QColor okGreen()     { return QColor("#4ec9b0"); }

    static QString fontUi()     { return QStringLiteral("Manrope"); }
    static QString fontMono()   { return QStringLiteral("JetBrains Mono"); }

    static void setMode(Mode m) { mode() = m; }
    static Mode  current()      { return mode(); }

    // QSS resource paths
    static QString darkSheet()         { return QStringLiteral(":/resources/theme_dark.qss"); }
    static QString lightSheet()        { return QStringLiteral(":/resources/theme_light.qss"); }
    static QString highContrastSheet() { return QStringLiteral(":/resources/theme_highcontrast.qss"); }

    static bool isSystemHighContrast() {
        QPalette p = QApplication::palette();
        int bgLum = p.color(QPalette::Window).lightness();
        int fgLum = p.color(QPalette::WindowText).lightness();
        return qAbs(fgLum - bgLum) > 200;
    }

    static QString sheetForMode(Mode m) {
        if (m == HighContrast) return highContrastSheet();
        return m == Dark ? darkSheet() : lightSheet();
    }

private:
    static Mode& mode() { static Mode m = Dark; return m; }
};

} // namespace gp
