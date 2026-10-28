// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace flashpack::ToolLocator {

// Returns the absolute path to "adb" / "fastboot" (".exe" is handled on Windows),
// or an empty string. Order: user folder -> bundled -> PATH -> common SDK locations.
QString find(const QString &tool);

QString customDir();
void setCustomDir(const QString &dir);

} // namespace flashpack::ToolLocator
