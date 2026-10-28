// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Always start in the user's home directory, wherever the binary lives.
    // (Bundled tools are located via applicationDirPath(), which is unaffected.)
    QDir::setCurrent(QDir::homePath());

    QApplication::setApplicationName(QStringLiteral("FlashPack"));
    QApplication::setOrganizationName(QStringLiteral("user14923929"));
    QApplication::setApplicationVersion(QStringLiteral(FLASHPACK_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("org.user14923929.flashpack"));

    flashpack::MainWindow window;
    window.show();
    return app.exec();
}
