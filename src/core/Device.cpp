// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/Device.h"

#include <QCoreApplication>

namespace flashpack {

QString modeToString(DeviceMode mode)
{
    switch (mode) {
    case DeviceMode::Adb:           return QCoreApplication::translate("flashpack", "ADB");
    case DeviceMode::Recovery:      return QCoreApplication::translate("flashpack", "Recovery");
    case DeviceMode::Sideload:      return QCoreApplication::translate("flashpack", "Sideload");
    case DeviceMode::Fastboot:      return QCoreApplication::translate("flashpack", "Fastboot");
    case DeviceMode::Unauthorized:  return QCoreApplication::translate("flashpack", "Unauthorized");
    case DeviceMode::NoPermissions: return QCoreApplication::translate("flashpack", "No permissions");
    case DeviceMode::Offline:       return QCoreApplication::translate("flashpack", "Offline");
    case DeviceMode::Unknown:       break;
    }
    return QCoreApplication::translate("flashpack", "Unknown");
}

} // namespace flashpack
