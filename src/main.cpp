#include "app/MainWindow.h"
#include "app/Theme.h"

#include <QApplication>
#include <QFont>

int main(int argc, char* argv[])
{
    // High-DPI is per-monitor aware by default in Qt 6 (rounding: Round).
    QApplication app(argc, argv);
    QApplication::setApplicationName(QString::fromUtf8(Theme::AppName));
    QApplication::setApplicationDisplayName(QString::fromUtf8(Theme::AppName));
    QApplication::setOrganizationName(QStringLiteral("MinecraftInjectedClientProxy"));
    QApplication::setApplicationVersion(QString::fromLatin1(Theme::AppVersion));

    Theme::applyApplicationFont();
    QApplication::setPalette(Theme::buildPalette());
    app.setStyleSheet(Theme::globalQss());

    MainWindow window;
    window.show();
    return QApplication::exec();
}
