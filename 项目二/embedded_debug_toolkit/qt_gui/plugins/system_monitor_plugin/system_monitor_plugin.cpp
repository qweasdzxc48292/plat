#include "system_monitor_plugin.h"

#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

SystemMonitorPlugin::SystemMonitorPlugin()
    : m_cpuBar(0),
      m_memBar(0),
      m_netLabel(0),
      m_msgLabel(0),
      m_timer(0),
      m_tick(0)
{
}

QString SystemMonitorPlugin::pluginName() const
{
    return "System Monitor";
}

QWidget *SystemMonitorPlugin::createWidget(QWidget *parent)
{
    QWidget *root = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(root);
    m_cpuBar = new QProgressBar(root);
    m_memBar = new QProgressBar(root);
    m_netLabel = new QLabel("Network: rx=0 tx=0", root);
    m_msgLabel = new QLabel("Waiting heartbeat...", root);

    m_cpuBar->setRange(0, 100);
    m_memBar->setRange(0, 100);
    m_cpuBar->setFormat("CPU: %p%");
    m_memBar->setFormat("MEM: %p%");

    layout->addWidget(new QLabel("System Resource Monitor", root));
    layout->addWidget(m_cpuBar);
    layout->addWidget(m_memBar);
    layout->addWidget(m_netLabel);
    layout->addWidget(m_msgLabel);
    layout->addStretch(1);

    m_timer = new QTimer(root);
    m_timer->setInterval(800);
    QObject::connect(m_timer, SIGNAL(timeout()), this, SLOT(refreshDemoStats()));
    m_timer->start();
    return root;
}

void SystemMonitorPlugin::onMessage(const QString &message)
{
    if (m_msgLabel) {
        m_msgLabel->setText(message);
    }
}

void SystemMonitorPlugin::refreshDemoStats()
{
    m_tick++;
    if (!m_cpuBar || !m_memBar || !m_netLabel) {
        return;
    }

    m_cpuBar->setValue((m_tick * 13) % 100);
    m_memBar->setValue(40 + (m_tick * 7) % 50);
    m_netLabel->setText(QString("Network: rx=%1KB tx=%2KB")
                            .arg(100 + m_tick * 3)
                            .arg(80 + m_tick * 2));
}
