// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/DeviceInfo.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace flashpack {

QString stripSlotSuffix(const QString &name)
{
    if (name.endsWith(QLatin1String("_a")) || name.endsWith(QLatin1String("_b")))
        return name.left(name.size() - 2);
    return name;
}

QString DeviceInfo::inactiveSlot() const
{
    if (!hasSlots())
        return QString();
    return currentSlot == QLatin1String("a") ? QStringLiteral("b") : QStringLiteral("a");
}

const PartitionEntry *DeviceInfo::find(const QString &name) const
{
    const QString base = stripSlotSuffix(name.trimmed());
    for (const PartitionEntry &e : partitions) {
        if (e.name == base || e.name == name)
            return &e;
    }
    return nullptr;
}

static qulonglong parseHex(QString text)
{
    text = text.trimmed();
    if (text.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        text = text.mid(2);
    bool ok = false;
    const qulonglong v = text.toULongLong(&ok, 16);
    return ok ? v : 0;
}

DeviceInfo parseDeviceInfo(const QMap<QString, QString> &vars)
{
    DeviceInfo info;

    QMap<QString, PartitionEntry> map;
    auto entry = [&map](const QString &raw) -> PartitionEntry & {
        const QString base = stripSlotSuffix(raw);
        PartitionEntry &e = map[base];
        e.name = base;
        if (base != raw)
            e.slotted = true;
        return e;
    };

    QSet<QString> seen;
    for (auto it = vars.cbegin(); it != vars.cend(); ++it) {
        const QString &key = it.key();
        const QString &value = it.value();

        if (key.startsWith(QLatin1String("partition-type:"))) {
            PartitionEntry &e = entry(key.mid(15));
            e.type = value;
            seen.insert(e.name);
        } else if (key.startsWith(QLatin1String("partition-size:"))) {
            PartitionEntry &e = entry(key.mid(15));
            e.size = std::max(e.size, parseHex(value));
            seen.insert(e.name);
        } else if (key.startsWith(QLatin1String("is-logical:"))) {
            PartitionEntry &e = entry(key.mid(11));
            if (value.compare(QLatin1String("yes"), Qt::CaseInsensitive) == 0)
                e.logical = true;
        }
    }

    for (const PartitionEntry &e : std::as_const(map)) {
        if (seen.contains(e.name))
            info.partitions << e;
    }
    std::sort(info.partitions.begin(), info.partitions.end(),
              [](const PartitionEntry &a, const PartitionEntry &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });

    QString slot = vars.value(QStringLiteral("current-slot")).trimmed();
    if (slot.startsWith(QLatin1Char('_')))
        slot.remove(0, 1);
    info.currentSlot = slot;
    info.slotCount = vars.value(QStringLiteral("slot-count")).trimmed().toInt();
    if (info.slotCount == 0) {
        for (const PartitionEntry &e : std::as_const(info.partitions)) {
            if (e.slotted) {
                info.slotCount = 2; // slotted partitions without an explicit count
                break;
            }
        }
    }
    return info;
}

QString resolveTarget(const DeviceInfo &info, const QString &name, bool useInactiveSlot)
{
    const QString typed = name.trimmed();
    if (!useInactiveSlot || typed.isEmpty() || !info.hasSlots())
        return typed;
    if (stripSlotSuffix(typed) != typed)
        return typed; // the user already chose a slot explicitly
    const PartitionEntry *e = info.find(typed);
    if (e && e->slotted)
        return typed + QLatin1Char('_') + info.inactiveSlot();
    return typed;
}

qulonglong imageFlashSize(const QString &path)
{
    const qulonglong fileSize = static_cast<qulonglong>(QFileInfo(path).size());

    // Android sparse image header (little endian): magic 0xED26FF3A,
    // blk_sz at offset 12, total_blks at offset 16.
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray h = f.read(28);
        if (h.size() == 28) {
            auto u32 = [&h](int off) {
                return static_cast<quint32>(static_cast<uchar>(h[off]))
                     | static_cast<quint32>(static_cast<uchar>(h[off + 1])) << 8
                     | static_cast<quint32>(static_cast<uchar>(h[off + 2])) << 16
                     | static_cast<quint32>(static_cast<uchar>(h[off + 3])) << 24;
            };
            if (u32(0) == 0xED26FF3Au)
                return static_cast<qulonglong>(u32(12)) * u32(16);
        }
    }
    return fileSize;
}

QString formatSize(qulonglong bytes)
{
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double v = static_cast<double>(bytes);
    int i = 0;
    while (v >= 1024.0 && i < 4) {
        v /= 1024.0;
        ++i;
    }
    if (i == 0)
        return QStringLiteral("%1 B").arg(bytes);
    return QString::number(v, 'f', 1) + QLatin1Char(' ') + QLatin1String(units[i]);
}

} // namespace flashpack
