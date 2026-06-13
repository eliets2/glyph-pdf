// SPDX-License-Identifier: Apache-2.0
#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include <QScreen>
#include <QStyleFactory>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QStyle>
#include <QPalette>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QLibraryInfo>
#include <QIcon>
#include <QSplashScreen>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

// New Glyph design UI
#include "GpMainWindow.h"
#include "util/GpTheme.h"

// Engine bootstrapping
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/TempFileManager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("GlyphPDF");
    QCoreApplication::setApplicationVersion("1.0.1");
    QCoreApplication::setOrganizationName("Glyph");

    // Application-wide window / taskbar icon (branding)
    app.setWindowIcon(QIcon(QStringLiteral(":/resources/branding/app_icon.png")));

    QCommandLineParser parser;
    parser.setApplicationDescription("Professional desktop PDF document platform");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", "PDF files to open", "[files...]");
    parser.process(app);
    const QStringList files = parser.positionalArguments();

    // Fusion base style — QSS overlays everything else
    app.setStyle(QStyleFactory::create("Fusion"));

    // === Branding splash screen (visible during engine bootstrap) ===
    QSplashScreen* splash = nullptr;
    {
        QPixmap splashPix(QStringLiteral(":/resources/branding/splash.png"));
        if (!splashPix.isNull()) {
            splashPix = splashPix.scaled(640, 420, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
            splash = new QSplashScreen(splashPix);
            splash->show();
            app.processEvents();
        }
    }

    // === Localization: Load translations ===
    QSettings settings;
    QString uiLang = settings.value("ui/language", "en").toString();
    QLocale locale(uiLang);
    QLocale::setDefault(locale);

    // Load Qt's own translations (standard dialogs, etc.)
    QTranslator qtTranslator;
    if (qtTranslator.load(locale, "qt", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // Load GlyphPDF translations
    QTranslator appTranslator;
    if (appTranslator.load(locale, "glyphpdf", "_", ":/translations")) {
        app.installTranslator(&appTranslator);
    }

    // === RTL layout support ===
    if (locale.textDirection() == Qt::RightToLeft) {
        app.setLayoutDirection(Qt::RightToLeft);
    }

    // Try to load bundled fonts (if present in resources/fonts/)
    for (const QString& f : {":/fonts/Manrope-Regular.ttf",
                             ":/fonts/Manrope-Bold.ttf",
                             ":/fonts/JetBrainsMono-Regular.ttf",
                             ":/fonts/JetBrainsMono-Bold.ttf"}) {
        if (QFile::exists(f)) QFontDatabase::addApplicationFont(f);
    }

    // === Theme selection ===
    QString themePref = settings.value("ui/theme", "dark").toString();
    gp::Theme::Mode themeMode = gp::Theme::Dark;

    if (themePref == "system") {
        if (gp::Theme::isSystemHighContrast()) {
            themeMode = gp::Theme::HighContrast;
        } else {
            // Detect system dark/light preference via palette
            QPalette sysPal = app.palette();
            themeMode = (sysPal.color(QPalette::Window).lightness() < 128)
                        ? gp::Theme::Dark : gp::Theme::Light;
        }
    } else if (themePref == "light") {
        themeMode = gp::Theme::Light;
    } else if (themePref == "highcontrast") {
        themeMode = gp::Theme::HighContrast;
    }

    gp::Theme::setMode(themeMode);
    QFile sheet(gp::Theme::sheetForMode(themeMode));
    if (sheet.open(QFile::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(sheet.readAll()));
    }

    // D6: Install temp file cleanup — atexit handler + stale cleanup
    TempFileManager::install();

    // Initialize engine infrastructure via Bootstrapper
    auto ctx = Bootstrapper::createContext();

    // Launch the new Glyph design UI
    gp::MainWindow mainWindow(&ctx);
    mainWindow.showMaximized();

    if (splash) {
        splash->finish(&mainWindow);
        splash->deleteLater();
    }

    // === System tray icon (branding + quick show/quit) ===
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        auto* tray = new QSystemTrayIcon(
            QIcon(QStringLiteral(":/resources/branding/tray_icon.png")), &mainWindow);
        tray->setToolTip(QStringLiteral("GlyphPDF"));
        auto* trayMenu = new QMenu(&mainWindow);
        QObject::connect(trayMenu->addAction(QObject::tr("Show GlyphPDF")),
                         &QAction::triggered, &mainWindow, [&mainWindow] {
                             mainWindow.showNormal();
                             mainWindow.raise();
                             mainWindow.activateWindow();
                         });
        QObject::connect(trayMenu->addAction(QObject::tr("Quit")),
                         &QAction::triggered, &app, &QApplication::quit);
        tray->setContextMenu(trayMenu);
        tray->show();
    }

    if (!files.isEmpty()) {
        mainWindow.openDocument(files.first());
    }

    return app.exec();
}
