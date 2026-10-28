// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Device.h"

#include <QList>
#include <QMap>
#include <QString>

namespace flashpack::parsers {

QList<Device> parseAdbDevices(const QString &text);         // `adb devices -l`
QList<Device> parseFastbootDevices(const QString &text);    // `fastboot devices`
QMap<QString, QString> parseGetVarAll(const QString &text); // `fastboot getvar all`

} // namespace flashpack::parsers
