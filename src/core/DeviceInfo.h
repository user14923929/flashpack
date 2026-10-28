// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QMap>
#include <QString>

namespace flashpack {

struct PartitionEntry {
    QString name;          // base name, slot suffix stripped ("boot", not "boot_a")
    QString type;
    qulonglong size = 0;   // bytes, 0 = unknown
    bool logical = false;  // lives inside `super`, needs fastbootd
    bool slotted = false;  // exists as <name>_a / <name>_b
};

struct DeviceInfo {
    QList<PartitionEntry> partitions; // sorted by name
    QString currentSlot;              // "a" / "b" (empty if not A/B)
    int slotCount = 0;

    bool hasSlots() const { return slotCount > 1 && !currentSlot.isEmpty(); }
    QString inactiveSlot() const;
    const PartitionEntry *find(const QString &name) const; // accepts "boot" or "boot_b"
};

// "boot_a" / "boot_b" -> "boot"
QString stripSlotSuffix(const QString &name);

// Builds DeviceInfo from the output of `fastboot getvar all`.
DeviceInfo parseDeviceInfo(const QMap<QString, QString> &vars);

// Name that is actually passed to fastboot: "boot" -> "boot_b" when the inactive slot
// is requested and the partition is slotted. Anything else is returned unchanged.
QString resolveTarget(const DeviceInfo &info, const QString &name, bool useInactiveSlot);

// Size the image will occupy on the partition (sparse images are measured unsparsed).
qulonglong imageFlashSize(const QString &path);

QString formatSize(qulonglong bytes);

} // namespace flashpack
