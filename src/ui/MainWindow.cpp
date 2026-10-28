// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/MainWindow.h"

#include "backend/CliBackend.h"
#include "backend/ToolLocator.h"
#include "core/DeviceManager.h"
#include "ui/PartitionDelegate.h"
#include "ui/WideComboBox.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSettings>
#include <QSplitter>
#include <QDir>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include <iterator>

namespace flashpack {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_backend(new CliBackend(this)),
      m_devices(new DeviceManager(m_backend, this))
{
    setWindowTitle(tr("FlashPack"));
    resize(1020, 700);
    setAcceptDrops(true);

    buildMenus();
    buildToolbar();
    buildCentral();

    connect(m_devices, &DeviceManager::devicesChanged, this, &MainWindow::onDevicesChanged);
    connect(m_backend, &IBackend::varsReady, this, &MainWindow::onVarsReady);
    connect(m_backend, &IBackend::taskStarted, this,
            [this](const QString &id, const QString &description) {
                appendLog(tr("\u25B6 [#%1] %2").arg(id, description));
            });
    connect(m_backend, &IBackend::taskOutput, this,
            [this](const QString &, const QString &line) { appendLog(line); });
    connect(m_backend, &IBackend::taskFinished, this,
            [this](const QString &id, bool ok, int exitCode) {
                appendLog(ok ? tr("\u2714 [#%1] finished").arg(id)
                             : tr("\u2718 [#%1] failed (exit code %2)").arg(id).arg(exitCode));
                m_devices->refreshNow();
            });

    updateToolsStatus();
    m_devices->start();
}

// ---------------------------------------------------------------- UI setup ---

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("Platform-tools folder\u2026"), this, &MainWindow::onChooseToolsDir);
    file->addSeparator();
    file->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About FlashPack"), this, [this] {
        QMessageBox::about(
            this, tr("About FlashPack"),
            tr("<b>FlashPack %1</b><br>A cross-platform GUI for adb and fastboot.<br><br>"
               "Licensed under the GNU GPL v3 or later.")
                .arg(QApplication::applicationVersion()));
    });
}

void MainWindow::buildToolbar()
{
    QToolBar *bar = addToolBar(tr("Reboot"));
    bar->setMovable(false);
    bar->addWidget(new QLabel(tr(" Reboot to: ")));
    bar->addAction(tr("System"), this, [this] { onReboot(RebootTarget::System); });
    bar->addAction(tr("Bootloader"), this, [this] { onReboot(RebootTarget::Bootloader); });
    bar->addAction(tr("Fastbootd"), this, [this] { onReboot(RebootTarget::Fastbootd); });
    bar->addAction(tr("Recovery"), this, [this] { onReboot(RebootTarget::Recovery); });
}

void MainWindow::buildCentral()
{
    // Left: device list.
    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->addWidget(new QLabel(tr("<b>Devices</b>")));
    m_deviceList = new QListWidget;
    connect(m_deviceList, &QListWidget::currentRowChanged, this,
            [this] { onDeviceSelectionChanged(); });
    leftLayout->addWidget(m_deviceList, 1);

    // Right-top: tabs.
    auto *tabs = new QTabWidget;

    auto *flashTab = new QWidget;
    auto *form = new QFormLayout(flashTab);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_partition = new WideComboBox;
    m_partition->setEditable(true);
    m_partition->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Fallback list, used until the device reports its real partitions.
    m_defaultPartitions = {QStringLiteral("boot"), QStringLiteral("init_boot"),
                           QStringLiteral("vendor_boot"), QStringLiteral("dtbo"),
                           QStringLiteral("vbmeta"), QStringLiteral("recovery"),
                           QStringLiteral("system"), QStringLiteral("vendor"),
                           QStringLiteral("product"), QStringLiteral("super"),
                           QStringLiteral("userdata")};
    m_partition->addItems(m_defaultPartitions);
    m_partition->setInsertPolicy(QComboBox::NoInsert); // typed names must not pollute the list
    m_partition->setToolTip(tr("Type to search; any partition name can be entered manually"));
    if (QCompleter *completer = m_partition->completer()) {
        completer->setFilterMode(Qt::MatchContains);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        completer->popup()->setItemDelegate(new PartitionDelegate(completer->popup()));
    }
    m_partition->view()->setItemDelegate(new PartitionDelegate(m_partition));
    form->addRow(tr("Partition:"), m_partition);

    m_inactiveSlot = new QCheckBox(tr("Use the inactive slot (A/B devices only)"));
    m_inactiveSlot->setEnabled(false);
    form->addRow(QString(), m_inactiveSlot);

    auto *imageRow = new QHBoxLayout;
    m_image = new QLineEdit;
    m_image->setPlaceholderText(tr("Choose an image or drop a file onto the window"));
    auto *browse = new QPushButton(tr("Browse\u2026"));
    connect(browse, &QPushButton::clicked, this, &MainWindow::onBrowseImage);
    imageRow->addWidget(m_image, 1);
    imageRow->addWidget(browse);
    form->addRow(tr("Image:"), imageRow);

    m_flashInfo = new QLabel;
    m_flashInfo->setWordWrap(true);
    form->addRow(QString(), m_flashInfo);

    connect(m_partition, &QComboBox::editTextChanged, this, [this] { updateFlashInfo(); });
    connect(m_image, &QLineEdit::textChanged, this, [this] { updateFlashInfo(); });
    connect(m_inactiveSlot, &QCheckBox::toggled, this, [this] { updateFlashInfo(); });

    auto *buttons = new QHBoxLayout;
    auto *flashBtn = new QPushButton(tr("Flash"));
    auto *eraseBtn = new QPushButton(tr("Erase"));
    connect(flashBtn, &QPushButton::clicked, this, &MainWindow::onFlash);
    connect(eraseBtn, &QPushButton::clicked, this, &MainWindow::onErase);
    buttons->addWidget(flashBtn);
    buttons->addWidget(eraseBtn);
    buttons->addStretch(1);
    form->addRow(buttons);
    tabs->addTab(flashTab, tr("Flash"));

    auto *varsTab = new QWidget;
    auto *varsLayout = new QVBoxLayout(varsTab);
    auto *varsTop = new QHBoxLayout;
    auto *readBtn = new QPushButton(tr("Read variables"));
    connect(readBtn, &QPushButton::clicked, this, &MainWindow::onReadVars);
    m_varsFilter = new QLineEdit;
    m_varsFilter->setPlaceholderText(tr("Filter\u2026"));
    m_varsFilter->setClearButtonEnabled(true);
    connect(m_varsFilter, &QLineEdit::textChanged, this, &MainWindow::applyVarsFilter);
    varsTop->addWidget(readBtn);
    varsTop->addWidget(m_varsFilter, 1);
    varsLayout->addLayout(varsTop);
    m_vars = new QTableWidget(0, 2);
    m_vars->setHorizontalHeaderLabels({tr("Variable"), tr("Value")});
    m_vars->horizontalHeader()->setStretchLastSection(true);
    m_vars->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_vars->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vars->setSortingEnabled(true);
    varsLayout->addWidget(m_vars, 1);
    tabs->addTab(varsTab, tr("Variables"));

    // Right-bottom: log.
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto *rightSplit = new QSplitter(Qt::Vertical);
    rightSplit->addWidget(tabs);
    rightSplit->addWidget(m_log);
    rightSplit->setStretchFactor(0, 3);
    rightSplit->setStretchFactor(1, 2);

    auto *mainSplit = new QSplitter(Qt::Horizontal);
    mainSplit->addWidget(left);
    mainSplit->addWidget(rightSplit);
    mainSplit->setStretchFactor(0, 1);
    mainSplit->setStretchFactor(1, 3);
    setCentralWidget(mainSplit);

    m_deviceCount = new QLabel;
    m_toolsStatus = new QLabel;
    statusBar()->addWidget(m_deviceCount);
    statusBar()->addPermanentWidget(m_toolsStatus);
}

// ------------------------------------------------------------------ events ---

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.first().isLocalFile())
        setImagePath(urls.first().toLocalFile());
}

void MainWindow::setImagePath(const QString &path)
{
    m_image->setText(path);
    // boot.img -> "boot", vbmeta.img -> "vbmeta", ...
    const int idx = m_partition->findText(QFileInfo(path).completeBaseName());
    if (idx >= 0)
        m_partition->setCurrentIndex(idx);
}

void MainWindow::onBrowseImage()
{
    // Start in the last used folder, falling back to the home directory.
    QSettings settings;
    const QString startDir =
        settings.value(QStringLiteral("lastImageDir"), QDir::homePath()).toString();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select image"), startDir,
        tr("Images (*.img *.bin *.zip);;All files (*)"));
    if (path.isEmpty())
        return;
    settings.setValue(QStringLiteral("lastImageDir"), QFileInfo(path).absolutePath());
    setImagePath(path);
}

void MainWindow::onChooseToolsDir()
{
    const QString current = ToolLocator::customDir();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select the platform-tools folder"),
        current.isEmpty() ? QDir::homePath() : current);
    if (dir.isEmpty())
        return;
    ToolLocator::setCustomDir(dir);
    updateToolsStatus();
    m_devices->refreshNow();
}

// ----------------------------------------------------------------- devices ---

void MainWindow::onDevicesChanged(const QList<Device> &devices)
{
    QString previous;
    if (const auto d = selectedDevice())
        previous = d->serial;

    m_current = devices;

    // Forget partition lists of devices that vanished or switched mode.
    for (auto it = m_partitions.begin(); it != m_partitions.end();) {
        bool keep = false;
        for (const Device &d : devices) {
            if (d.serial == it.key() && d.mode == it->mode) {
                keep = true;
                break;
            }
        }
        it = keep ? std::next(it) : m_partitions.erase(it);
    }

    QSignalBlocker blocker(m_deviceList); // one explicit update below
    m_deviceList->clear();
    int reselect = devices.isEmpty() ? -1 : 0;
    for (int i = 0; i < devices.size(); ++i) {
        const Device &d = devices.at(i);
        const QString name = d.model.isEmpty() ? d.serial : d.model + QStringLiteral(" \u00B7 ") + d.serial;
        auto *item = new QListWidgetItem(QStringLiteral("%1  [%2]").arg(name, modeToString(d.mode)));
        if (d.mode == DeviceMode::NoPermissions)
            item->setToolTip(tr("Missing permissions. On Linux, install the udev rules for Android devices."));
        else if (d.mode == DeviceMode::Unauthorized)
            item->setToolTip(tr("Confirm the USB debugging prompt on the device."));
        m_deviceList->addItem(item);
        if (d.serial == previous)
            reselect = i;
    }
    if (reselect >= 0)
        m_deviceList->setCurrentRow(reselect);

    blocker.unblock();
    onDeviceSelectionChanged();

    m_deviceCount->setText(tr("%n device(s) connected", nullptr, devices.size()));
}

void MainWindow::onDeviceSelectionChanged()
{
    const auto dev = selectedDevice();
    if (!dev || dev->mode != DeviceMode::Fastboot) {
        applyPartitions(DeviceInfo());
        return;
    }

    PartitionState &st = m_partitions[dev->serial];
    if (!st.requested) {
        st.mode = dev->mode;
        st.requested = true;
        m_backend->getVarAll(dev->serial); // result arrives in onVarsReady()
    }
    applyPartitions(st.info);
}

const DeviceInfo *MainWindow::currentInfo() const
{
    const auto dev = selectedDevice();
    if (!dev)
        return nullptr;
    const auto it = m_partitions.constFind(dev->serial);
    return it == m_partitions.constEnd() ? nullptr : &it->info;
}

QString MainWindow::targetPartition() const
{
    const QString typed = m_partition->currentText().trimmed();
    const DeviceInfo *info = currentInfo();
    return info ? resolveTarget(*info, typed, m_inactiveSlot->isChecked()) : typed;
}

void MainWindow::applyPartitions(const DeviceInfo &info)
{
    const QString keep = m_partition->currentText();
    {
        QSignalBlocker blocker(m_partition);
        m_partition->clear();
        if (info.partitions.isEmpty()) {
            m_partition->addItems(m_defaultPartitions);
        } else {
            for (const PartitionEntry &e : info.partitions) {
                const int row = m_partition->count();
                m_partition->addItem(e.name);
                QStringList extra;
                if (e.size)
                    extra << formatSize(e.size);
                if (e.logical)
                    extra << tr("logical");
                m_partition->setItemData(row, extra.join(QStringLiteral(" \u00B7 ")),
                                         PartitionDelegate::SizeTextRole);
                if (e.logical)
                    m_partition->setItemData(row, tr("Logical partition: flash it from fastbootd"),
                                             Qt::ToolTipRole);
            }
        }
        if (!keep.isEmpty())
            m_partition->setCurrentText(keep);
    }

    // Make the popups wide enough for "name ... size", whatever the combo's own width is.
    {
        QAbstractItemView *view = m_partition->view();
        const int width = view->sizeHintForColumn(0)
                          + view->verticalScrollBar()->sizeHint().width() + 24;
        view->setMinimumWidth(width);
        if (QCompleter *completer = m_partition->completer())
            completer->popup()->setMinimumWidth(width);
    }

    const bool abDevice = info.hasSlots();
    if (!abDevice)
        m_inactiveSlot->setChecked(false);
    m_inactiveSlot->setEnabled(abDevice);
    m_inactiveSlot->setText(abDevice
        ? tr("Use the inactive slot (current: %1, target: %2)").arg(info.currentSlot, info.inactiveSlot())
        : tr("Use the inactive slot (A/B devices only)"));
    updateFlashInfo();
}

void MainWindow::updateFlashInfo()
{
    if (!m_flashInfo)
        return;

    const QString typed = m_partition->currentText().trimmed();
    if (typed.isEmpty()) {
        m_flashInfo->clear();
        return;
    }

    const DeviceInfo *info = currentInfo();
    const PartitionEntry *entry = info ? info->find(typed) : nullptr;
    const qulonglong partSize = entry ? entry->size : 0;

    QStringList parts;
    parts << tr("Target: %1").arg(targetPartition());
    if (partSize)
        parts << tr("Partition: %1").arg(formatSize(partSize));

    bool tooLarge = false;
    const QString image = m_image->text().trimmed();
    if (!image.isEmpty() && QFileInfo::exists(image)) {
        const qulonglong imageSize = imageFlashSize(image);
        parts << tr("Image: %1").arg(formatSize(imageSize));
        if (partSize && imageSize > partSize) {
            tooLarge = true;
            parts << tr("\u2718 larger than the partition by %1").arg(formatSize(imageSize - partSize));
        } else if (partSize) {
            parts << tr("\u2714 fits");
        }
    }

    m_flashInfo->setText(parts.join(QStringLiteral(" \u00B7 ")));
    m_flashInfo->setStyleSheet(tooLarge ? QStringLiteral("color: #c0392b;") : QString());
}

std::optional<Device> MainWindow::selectedDevice() const
{
    const int row = m_deviceList ? m_deviceList->currentRow() : -1;
    if (row < 0 || row >= m_current.size())
        return std::nullopt;
    return m_current.at(row);
}

std::optional<Device> MainWindow::requireFastbootDevice()
{
    const auto dev = selectedDevice();
    if (!dev) {
        QMessageBox::information(this, tr("No device"), tr("Select a device first."));
        return std::nullopt;
    }
    if (dev->mode != DeviceMode::Fastboot) {
        QMessageBox::information(this, tr("Device is not in fastboot mode"),
                                 tr("Reboot the device to the bootloader first "
                                    "(toolbar \u2192 Bootloader)."));
        return std::nullopt;
    }
    return dev;
}

// ----------------------------------------------------------------- actions ---

bool MainWindow::isDangerousPartition(const QString &partition)
{
    static const QStringList critical = {
        QStringLiteral("bootloader"), QStringLiteral("aboot"), QStringLiteral("abl"),
        QStringLiteral("xbl"),        QStringLiteral("xbl_config"), QStringLiteral("sbl1"),
        QStringLiteral("modem"),      QStringLiteral("radio"), QStringLiteral("persist"),
        QStringLiteral("efs"),        QStringLiteral("modemst1"), QStringLiteral("modemst2"),
        QStringLiteral("fsg"),        QStringLiteral("tz"), QStringLiteral("hyp"),
        QStringLiteral("rpm"),        QStringLiteral("devcfg"), QStringLiteral("keymaster")};

    QString name = partition.trimmed().toLower();
    if (name.endsWith(QLatin1String("_a")) || name.endsWith(QLatin1String("_b")))
        name.chop(2);
    return critical.contains(name);
}

bool MainWindow::typedConfirm(const QString &title, const QString &text, const QString &keyword)
{
    bool ok = false;
    const QString typed = QInputDialog::getText(
        this, title, text + QStringLiteral("\n\n") + tr("Type %1 to confirm:").arg(keyword),
        QLineEdit::Normal, QString(), &ok);
    return ok && typed.trimmed() == keyword;
}

void MainWindow::onFlash()
{
    const auto dev = requireFastbootDevice();
    if (!dev)
        return;

    const QString partition = targetPartition();
    const QString image = m_image->text().trimmed();
    if (partition.isEmpty() || image.isEmpty() || !QFileInfo::exists(image)) {
        QMessageBox::warning(this, tr("Cannot flash"),
                             tr("Choose a partition and an existing image file."));
        return;
    }

    // Does the image fit?
    if (const DeviceInfo *info = currentInfo()) {
        const PartitionEntry *entry = info->find(partition);
        if (entry && entry->size) {
            const qulonglong imageSize = imageFlashSize(image);
            if (imageSize > entry->size) {
                const auto answer = QMessageBox::warning(
                    this, tr("Image is too large"),
                    tr("The image (%1) is larger than partition '%2' (%3). "
                       "Flashing will most likely fail.\n\nFlash anyway?")
                        .arg(formatSize(imageSize), partition, formatSize(entry->size)),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer != QMessageBox::Yes)
                    return;
            }
        }
    }

    if (isDangerousPartition(partition)
        && !typedConfirm(tr("Dangerous partition"),
                         tr("Flashing '%1' can permanently brick this device.").arg(partition),
                         QStringLiteral("FLASH")))
        return;

    m_backend->flash(dev->serial, partition, image);
}

void MainWindow::onErase()
{
    const auto dev = requireFastbootDevice();
    if (!dev)
        return;

    const QString partition = targetPartition();
    if (partition.isEmpty()) {
        QMessageBox::warning(this, tr("Cannot erase"), tr("Choose a partition."));
        return;
    }
    if (!typedConfirm(tr("Erase partition"),
                      tr("All data on '%1' will be lost.").arg(partition),
                      QStringLiteral("ERASE")))
        return;

    m_backend->erase(dev->serial, partition);
}

void MainWindow::onReadVars()
{
    if (const auto dev = requireFastbootDevice())
        m_backend->getVarAll(dev->serial);
}

void MainWindow::onVarsReady(const QString &serial, const QMap<QString, QString> &vars)
{
    const DeviceInfo info = parseDeviceInfo(vars);
    const auto st = m_partitions.find(serial);
    if (st != m_partitions.end())
        st->info = info;
    if (const auto dev = selectedDevice(); dev && dev->serial == serial)
        applyPartitions(info);

    m_vars->setSortingEnabled(false);
    m_vars->setRowCount(0);
    for (auto it = vars.cbegin(); it != vars.cend(); ++it) {
        const int row = m_vars->rowCount();
        m_vars->insertRow(row);
        m_vars->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_vars->setItem(row, 1, new QTableWidgetItem(it.value()));
    }
    m_vars->setSortingEnabled(true);
    applyVarsFilter(m_varsFilter->text());
}

void MainWindow::applyVarsFilter(const QString &text)
{
    for (int row = 0; row < m_vars->rowCount(); ++row) {
        const bool match = text.isEmpty()
            || m_vars->item(row, 0)->text().contains(text, Qt::CaseInsensitive)
            || m_vars->item(row, 1)->text().contains(text, Qt::CaseInsensitive);
        m_vars->setRowHidden(row, !match);
    }
}

void MainWindow::onReboot(RebootTarget target)
{
    const auto dev = selectedDevice();
    if (!dev) {
        QMessageBox::information(this, tr("No device"), tr("Select a device first."));
        return;
    }
    switch (dev->mode) {
    case DeviceMode::Adb:
    case DeviceMode::Recovery:
    case DeviceMode::Sideload:
    case DeviceMode::Fastboot:
        m_backend->reboot(*dev, target);
        break;
    default:
        QMessageBox::information(this, tr("Cannot reboot"),
                                 tr("The device is not ready (%1).").arg(modeToString(dev->mode)));
        break;
    }
}

// ----------------------------------------------------------------- helpers ---

void MainWindow::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void MainWindow::updateToolsStatus()
{
    QString details;
    if (m_backend->toolsAvailable(&details)) {
        m_toolsStatus->setText(tr("adb and fastboot found"));
        m_toolsStatus->setStyleSheet(QString());
    } else {
        m_toolsStatus->setText(details + tr(" \u2014 File \u2192 Platform-tools folder\u2026"));
        m_toolsStatus->setStyleSheet(QStringLiteral("color: #c0392b;"));
    }
}

} // namespace flashpack
