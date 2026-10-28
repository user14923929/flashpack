// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/ToolLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace flashpack::ToolLocator {

static const char kSettingsKey[] = "platformToolsDir";

QString customDir()
{
    return QSettings().value(QLatin1String(kSettingsKey)).toString();
}

void setCustomDir(const QString &dir)
{
    QSettings().setValue(QLatin1String(kSettingsKey), dir);
}

static QStringList priorityDirs()
{
    QStringList dirs;
    const QString custom = customDir();
    if (!custom.isEmpty())
        dirs << custom;
    const QString app = QCoreApplication::applicationDirPath();
    dirs << app + QLatin1String("/platform-tools") << app;
    return dirs;
}

static QStringList fallbackDirs()
{
    QStringList dirs;
    for (const char *var : {"ANDROID_HOME", "ANDROID_SDK_ROOT"}) {
        const QString v = qEnvironmentVariable(var);
        if (!v.isEmpty())
            dirs << v + QLatin1String("/platform-tools");
    }
    const QString home = QDir::homePath();
    dirs << home + QLatin1String("/Android/Sdk/platform-tools")              // Linux
         << home + QLatin1String("/Library/Android/sdk/platform-tools")       // macOS
         << home + QLatin1String("/AppData/Local/Android/Sdk/platform-tools") // Windows
         << QStringLiteral("/opt/android-sdk/platform-tools")
         << QStringLiteral("/opt/homebrew/bin")
         << QStringLiteral("/usr/local/bin");
    return dirs;
}

QString find(const QString &tool)
{
    QString path = QStandardPaths::findExecutable(tool, priorityDirs());
    if (path.isEmpty())
        path = QStandardPaths::findExecutable(tool); // PATH
    if (path.isEmpty())
        path = QStandardPaths::findExecutable(tool, fallbackDirs());
    return path;
}

} // namespace flashpack::ToolLocator
