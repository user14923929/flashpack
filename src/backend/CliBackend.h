// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "backend/IBackend.h"

#include <QHash>
#include <QQueue>
#include <QSet>
#include <QStringList>

#include <functional>

namespace flashpack {

// Backend that drives the official `adb` and `fastboot` binaries via QProcess.
// Tasks are serialized per device (one at a time per serial, devices in parallel).
class CliBackend : public IBackend {
    Q_OBJECT
public:
    explicit CliBackend(QObject *parent = nullptr);

    bool toolsAvailable(QString *details = nullptr) const override;
    void refreshDevices() override;

    QString getVarAll(const QString &serial) override;
    QString flash(const QString &serial, const QString &partition,
                  const QString &imagePath) override;
    QString erase(const QString &serial, const QString &partition) override;
    QString reboot(const Device &device, RebootTarget target) override;

private:
    struct Task {
        QString id;
        QString serial;
        QString description;
        QString tool;
        QStringList args;
        bool captureVars = false;
    };

    QString enqueue(Task task);
    void startNext(const QString &serial);
    void runCapture(const QString &tool, const QStringList &args,
                    std::function<void(const QString &)> done);
    void finishRefresh();

    QHash<QString, QQueue<Task>> m_queues;
    QSet<QString> m_busy;
    QHash<QString, QString> m_captured;
    int m_nextId = 1;

    bool m_refreshing = false;
    int m_pending = 0;
    QList<Device> m_adb;
    QList<Device> m_fastboot;
};

} // namespace flashpack
