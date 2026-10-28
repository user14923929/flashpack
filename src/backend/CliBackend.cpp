// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/CliBackend.h"

#include "backend/Parsers.h"
#include "backend/ToolLocator.h"

#include <QByteArray>
#include <QProcess>
#include <QTimer>

#include <algorithm>
#include <memory>

namespace flashpack {

namespace {

// fastboot prints progress using '\r', so treat both as line breaks.
int indexOfLineBreak(const QByteArray &buf)
{
    for (int i = 0; i < buf.size(); ++i) {
        if (buf[i] == '\n' || buf[i] == '\r')
            return i;
    }
    return -1;
}

} // namespace

CliBackend::CliBackend(QObject *parent) : IBackend(parent) {}

bool CliBackend::toolsAvailable(QString *details) const
{
    QStringList missing;
    for (const QString &tool : {QStringLiteral("adb"), QStringLiteral("fastboot")}) {
        if (ToolLocator::find(tool).isEmpty())
            missing << tool;
    }
    if (details)
        *details = missing.isEmpty() ? QString()
                                     : tr("Missing tools: %1").arg(missing.join(QStringLiteral(", ")));
    return missing.isEmpty();
}

// ---------------------------------------------------------------- devices ---

void CliBackend::refreshDevices()
{
    if (m_refreshing)
        return;
    m_refreshing = true;
    m_pending = 2;
    m_adb.clear();
    m_fastboot.clear();

    runCapture(QStringLiteral("adb"), {QStringLiteral("devices"), QStringLiteral("-l")},
               [this](const QString &out) {
                   m_adb = parsers::parseAdbDevices(out);
                   finishRefresh();
               });
    runCapture(QStringLiteral("fastboot"), {QStringLiteral("devices")},
               [this](const QString &out) {
                   m_fastboot = parsers::parseFastbootDevices(out);
                   finishRefresh();
               });
}

void CliBackend::finishRefresh()
{
    if (--m_pending > 0)
        return;
    m_refreshing = false;

    QList<Device> all = m_adb + m_fastboot;
    std::sort(all.begin(), all.end(),
              [](const Device &a, const Device &b) { return a.serial < b.serial; });
    emit devicesRefreshed(all);
}

void CliBackend::runCapture(const QString &tool, const QStringList &args,
                            std::function<void(const QString &)> done)
{
    const QString exe = ToolLocator::find(tool);
    if (exe.isEmpty()) {
        done(QString());
        return;
    }

    auto *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::MergedChannels); // fastboot writes to stderr
    connect(p, &QProcess::finished, this, [p, done](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(p->readAll());
        p->deleteLater();
        done(out);
    });
    connect(p, &QProcess::errorOccurred, this, [p, done](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            p->deleteLater();
            done(QString());
        }
    });
    // Never let a hung `adb start-server` block device refreshes forever.
    QTimer::singleShot(10000, p, [p] {
        if (p->state() != QProcess::NotRunning)
            p->kill();
    });
    p->start(exe, args);
}

// ------------------------------------------------------------------ tasks ---

QString CliBackend::getVarAll(const QString &serial)
{
    Task t;
    t.serial = serial;
    t.description = tr("Read variables");
    t.tool = QStringLiteral("fastboot");
    t.args = {QStringLiteral("-s"), serial, QStringLiteral("getvar"), QStringLiteral("all")};
    t.captureVars = true;
    return enqueue(t);
}

QString CliBackend::flash(const QString &serial, const QString &partition,
                          const QString &imagePath)
{
    Task t;
    t.serial = serial;
    t.description = tr("Flash %1").arg(partition);
    t.tool = QStringLiteral("fastboot");
    t.args = {QStringLiteral("-s"), serial, QStringLiteral("flash"), partition, imagePath};
    return enqueue(t);
}

QString CliBackend::erase(const QString &serial, const QString &partition)
{
    Task t;
    t.serial = serial;
    t.description = tr("Erase %1").arg(partition);
    t.tool = QStringLiteral("fastboot");
    t.args = {QStringLiteral("-s"), serial, QStringLiteral("erase"), partition};
    return enqueue(t);
}

QString CliBackend::reboot(const Device &device, RebootTarget target)
{
    QString arg;
    switch (target) {
    case RebootTarget::System:     break;
    case RebootTarget::Bootloader: arg = QStringLiteral("bootloader"); break;
    case RebootTarget::Fastbootd:  arg = QStringLiteral("fastboot"); break;
    case RebootTarget::Recovery:   arg = QStringLiteral("recovery"); break;
    }

    Task t;
    t.serial = device.serial;
    t.description = tr("Reboot (%1)").arg(arg.isEmpty() ? tr("system") : arg);
    t.tool = device.mode == DeviceMode::Fastboot ? QStringLiteral("fastboot")
                                                 : QStringLiteral("adb");
    t.args = {QStringLiteral("-s"), device.serial, QStringLiteral("reboot")};
    if (!arg.isEmpty())
        t.args << arg;
    return enqueue(t);
}

QString CliBackend::enqueue(Task task)
{
    task.id = QString::number(m_nextId++);
    const QString id = task.id;
    const QString serial = task.serial;
    m_queues[serial].enqueue(task);
    if (!m_busy.contains(serial))
        startNext(serial);
    return id;
}

void CliBackend::startNext(const QString &serial)
{
    if (!m_queues.contains(serial) || m_queues[serial].isEmpty()) {
        m_busy.remove(serial);
        m_queues.remove(serial);
        return;
    }

    const Task task = m_queues[serial].dequeue();
    m_busy.insert(serial);
    emit taskStarted(task.id, task.description);

    const QString exe = ToolLocator::find(task.tool);
    if (exe.isEmpty()) {
        emit taskOutput(task.id, tr("Error: '%1' was not found. Set the platform-tools folder "
                                    "in File \xE2\x86\x92 Platform-tools folder.")
                                     .arg(task.tool));
        emit taskFinished(task.id, false, -1);
        startNext(serial);
        return;
    }

    const QString id = task.id;
    const bool capture = task.captureVars;

    auto *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::MergedChannels);
    auto buffer = std::make_shared<QByteArray>();

    auto emitLine = [this, id, capture](const QByteArray &raw) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty())
            return;
        if (capture) {
            // getvar output is large; keep it out of the log unless the task fails.
            m_captured[id] += line + QLatin1Char('\n');
            return;
        }
        emit taskOutput(id, line);
    };

    connect(p, &QProcess::readyRead, this, [p, buffer, emitLine] {
        buffer->append(p->readAll());
        int idx;
        while ((idx = indexOfLineBreak(*buffer)) >= 0) {
            emitLine(buffer->left(idx));
            buffer->remove(0, idx + 1);
        }
    });

    connect(p, &QProcess::finished, this,
            [this, p, buffer, emitLine, id, serial, capture](int exitCode,
                                                            QProcess::ExitStatus status) {
                buffer->append(p->readAll());
                emitLine(*buffer);
                buffer->clear();
                p->deleteLater();

                const bool ok = status == QProcess::NormalExit && exitCode == 0;
                if (capture) {
                    const QString text = m_captured.take(id);
                    if (!ok) {
                        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                        for (const QString &line : lines)
                            emit taskOutput(id, line);
                    }
                    emit varsReady(serial, parsers::parseGetVarAll(text));
                }
                emit taskFinished(id, ok, exitCode);
                startNext(serial);
            });

    connect(p, &QProcess::errorOccurred, this,
            [this, p, id, serial, capture](QProcess::ProcessError err) {
                if (err != QProcess::FailedToStart)
                    return;
                m_captured.remove(id);
                p->deleteLater();
                emit taskOutput(id, tr("Error: failed to start the process."));
                emit taskFinished(id, false, -1);
                Q_UNUSED(capture)
                startNext(serial);
            });

    emit taskOutput(id, QStringLiteral("$ ") + task.tool + QLatin1Char(' ')
                            + task.args.join(QLatin1Char(' ')));
    p->start(exe, task.args);
}

} // namespace flashpack
