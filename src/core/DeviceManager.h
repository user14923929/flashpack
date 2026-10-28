// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "backend/IBackend.h"
#include "core/Device.h"

#include <QList>
#include <QObject>
#include <QTimer>

namespace flashpack {

// Polls the backend and emits devicesChanged() only when the list really changed.
class DeviceManager : public QObject {
    Q_OBJECT
public:
    explicit DeviceManager(IBackend *backend, QObject *parent = nullptr);

    void start(int intervalMs = 2000);
    void stop();
    void refreshNow();

    const QList<Device> &devices() const { return m_devices; }

signals:
    void devicesChanged(const QList<flashpack::Device> &devices);

private:
    IBackend *m_backend;
    QTimer m_timer;
    QList<Device> m_devices;
};

} // namespace flashpack
