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

// New Glyph design UI
#include "GpMainWindow.h"
#include "util/GpTheme.h"

// Engine bootstrapping
#include "app/Bootstrapper.h"
#include "core/AppContext.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("GlyphPDF");
    QCoreApplication::setApplicationVersion("0.2.0");
    QCoreApplication::setOrganizationName("Glyph");

    QCommandLineParser parser;
    parser.setApplicationDescription("Professional desktop PDF document platform");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", "PDF files to open", "[files...]");
    parser.process(app);
    const QStringList files = parser.positionalArguments();

    // Fusion base style — QSS overlays everything else
    app.setStyle(QStyleFactory::create("Fusion"));

    // Try to load bundled fonts (if present in resources/fonts/)
    for (const QString& f : {":/fonts/Manrope-Regular.ttf",
                             ":/fonts/Manrope-Bold.ttf",
                             ":/fonts/JetBrainsMono-Regular.ttf",
                             ":/fonts/JetBrainsMono-Bold.ttf"}) {
        if (QFile::exists(f)) QFontDatabase::addApplicationFont(f);
    }

    // Apply the Glyph dark theme QSS
    QFile sheet(gp::Theme::darkSheet());
    if (sheet.open(QFile::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(sheet.readAll()));
    }

    // Initialize engine infrastructure via Bootstrapper
    auto ctx = Bootstrapper::createContext();

    // Launch the new Glyph design UI
    gp::MainWindow mainWindow(&ctx);
    mainWindow.showMaximized();

    if (!files.isEmpty()) {
        mainWindow.openDocument(files.first());
    }

    return app.exec();
}
