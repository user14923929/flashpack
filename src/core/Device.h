// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace flashpack {

enum class DeviceMode {
    Unknown,
    Adb,
    Recovery,
    Sideload,
    Fastboot,      // bootloader fastboot or userspace fastbootd
    Unauthorized,
    NoPermissions, // Linux: missing udev rules
    Offline
};

enum class RebootTarget { System, Bootloader, Fastbootd, Recovery };

struct Device {
    QString serial;
    DeviceMode mode = DeviceMode::Unknown;
    QString model;

    bool operator==(const Device &o) const {
        return serial == o.serial && mode == o.mode && model == o.model;
    }
    bool operator!=(const Device &o) const { return !(*this == o); }
};

QString modeToString(DeviceMode mode);

} // namespace flashpack
