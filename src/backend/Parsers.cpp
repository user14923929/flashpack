// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/Parsers.h"

#include <QRegularExpression>

namespace flashpack::parsers {

QList<Device> parseAdbDevices(const QString &text)
{
    static const QRegularExpression lineRe(QStringLiteral("^(\\S+)\\s+(\\S+)(?:\\s+(.*))?$"));
    static const QRegularExpression modelRe(QStringLiteral("\\bmodel:(\\S+)"));

    QList<Device> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('*'))
            || line.startsWith(QLatin1String("List of devices")))
            continue;

        const QRegularExpressionMatch m = lineRe.match(line);
        if (!m.hasMatch())
            continue;

        const QString state = m.captured(2);
        const QString rest = m.captured(3);

        Device d;
        d.serial = m.captured(1);
        if (state == QLatin1String("device"))            d.mode = DeviceMode::Adb;
        else if (state == QLatin1String("recovery"))     d.mode = DeviceMode::Recovery;
        else if (state == QLatin1String("sideload"))     d.mode = DeviceMode::Sideload;
        else if (state == QLatin1String("unauthorized")) d.mode = DeviceMode::Unauthorized;
        else if (state == QLatin1String("offline"))      d.mode = DeviceMode::Offline;
        else if (state == QLatin1String("no") && rest.startsWith(QLatin1String("permissions")))
            d.mode = DeviceMode::NoPermissions;
        else
            continue; // not a device line (e.g. "adb server is out of date")

        const QRegularExpressionMatch mm = modelRe.match(rest);
        if (mm.hasMatch())
            d.model = mm.captured(1);
        out << d;
    }
    return out;
}

QList<Device> parseFastbootDevices(const QString &text)
{
    static const QRegularExpression lineRe(QStringLiteral("^(\\S+)\\s+(fastboot|fastbootd)\\b"));

    QList<Device> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QRegularExpressionMatch m = lineRe.match(raw.trimmed());
        if (!m.hasMatch())
            continue;
        Device d;
        d.serial = m.captured(1);
        d.mode = DeviceMode::Fastboot;
        out << d;
    }
    return out;
}

QMap<QString, QString> parseGetVarAll(const QString &text)
{
    // "(bootloader) key: value" (older) or "key: value" (newer fastboot).
    // Keys may contain colons ("partition-size:boot_a"), so split on ": ".
    static const QRegularExpression lineRe(
        QStringLiteral("^(?:\\(bootloader\\)\\s+)?(.*?)(?::\\s+|:$)(.*)$"));

    QMap<QString, QString> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1String("Finished"))
            || line.startsWith(QLatin1String("all:")))
            continue;
        const QRegularExpressionMatch m = lineRe.match(line);
        if (!m.hasMatch() || m.captured(1).isEmpty())
            continue;
        out.insert(m.captured(1), m.captured(2));
    }
    return out;
}

} // namespace flashpack::parsers
