// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Device.h"
#include "core/DeviceInfo.h"

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QStringList>

#include <optional>

class QCheckBox;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTableWidget;

namespace flashpack {

class CliBackend;
class DeviceManager;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void buildMenus();
    void buildToolbar();
    void buildCentral();

    void onDevicesChanged(const QList<Device> &devices);
    void onVarsReady(const QString &serial, const QMap<QString, QString> &vars);
    void onDeviceSelectionChanged();
    void applyPartitions(const DeviceInfo &info);
    void updateFlashInfo();
    const DeviceInfo *currentInfo() const;
    QString targetPartition() const;
    void onFlash();
    void onErase();
    void onReadVars();
    void onReboot(RebootTarget target);
    void onBrowseImage();
    void onChooseToolsDir();
    void setImagePath(const QString &path);

    std::optional<Device> selectedDevice() const;
    std::optional<Device> requireFastbootDevice();
    bool typedConfirm(const QString &title, const QString &text, const QString &keyword);
    static bool isDangerousPartition(const QString &partition);

    void appendLog(const QString &line);
    void updateToolsStatus();
    void applyVarsFilter(const QString &text);

    CliBackend *m_backend;
    DeviceManager *m_devices;
    QList<Device> m_current;

    // What each fastboot device reported via `getvar all` (partitions, slots, sizes).
    struct PartitionState {
        DeviceMode mode = DeviceMode::Unknown;
        bool requested = false;
        DeviceInfo info;
    };
    QHash<QString, PartitionState> m_partitions;
    QStringList m_defaultPartitions;

    QListWidget *m_deviceList = nullptr;
    QComboBox *m_partition = nullptr;
    QLineEdit *m_image = nullptr;
    QCheckBox *m_inactiveSlot = nullptr;
    QLabel *m_flashInfo = nullptr;
    QLineEdit *m_varsFilter = nullptr;
    QTableWidget *m_vars = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLabel *m_toolsStatus = nullptr;
    QLabel *m_deviceCount = nullptr;
};

} // namespace flashpack
