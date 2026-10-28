// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Device.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

namespace flashpack {

// Everything the UI needs from the "outside world".
// Today: CliBackend (adb + fastboot binaries). Later: a native USB backend.
class IBackend : public QObject {
    Q_OBJECT
public:
    explicit IBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~IBackend() override = default;

    // True when all required tools were found; otherwise fills `details`.
    virtual bool toolsAvailable(QString *details = nullptr) const = 0;

    virtual void refreshDevices() = 0;

    // The methods below queue a task and return its id.
    virtual QString getVarAll(const QString &serial) = 0;
    virtual QString flash(const QString &serial, const QString &partition,
                          const QString &imagePath) = 0;
    virtual QString erase(const QString &serial, const QString &partition) = 0;
    virtual QString reboot(const Device &device, RebootTarget target) = 0;

signals:
    void devicesRefreshed(const QList<flashpack::Device> &devices);
    void taskStarted(const QString &taskId, const QString &description);
    void taskOutput(const QString &taskId, const QString &line);
    void taskFinished(const QString &taskId, bool success, int exitCode);
    void varsReady(const QString &serial, const QMap<QString, QString> &vars);
};

} // namespace flashpack
