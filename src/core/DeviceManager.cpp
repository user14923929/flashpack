// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/DeviceManager.h"

namespace flashpack {

DeviceManager::DeviceManager(IBackend *backend, QObject *parent)
    : QObject(parent), m_backend(backend)
{
    connect(m_backend, &IBackend::devicesRefreshed, this,
            [this](const QList<Device> &devices) {
                if (devices == m_devices)
                    return;
                m_devices = devices;
                emit devicesChanged(m_devices);
            });
    connect(&m_timer, &QTimer::timeout, this, &DeviceManager::refreshNow);
}

void DeviceManager::start(int intervalMs)
{
    m_timer.start(intervalMs);
    refreshNow();
}

void DeviceManager::stop()
{
    m_timer.stop();
}

void DeviceManager::refreshNow()
{
    m_backend->refreshDevices();
}

} // namespace flashpack
