#ifndef SYSTEM_MONITOR_PLUGIN_H
#define SYSTEM_MONITOR_PLUGIN_H

#include <QObject>

#include "../../debug_workbench/idebug_tool_plugin.h"

class QLabel;
class QProgressBar;
class QTimer;
class QWidget;

class SystemMonitorPlugin : public QObject, public IDebugToolPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IDebugToolPlugin_iid)
    Q_INTERFACES(IDebugToolPlugin)

public:
    SystemMonitorPlugin();
    QString pluginName() const;
    QWidget *createWidget(QWidget *parent = 0);
    void onMessage(const QString &message);

private slots:
    void refreshDemoStats();

private:
    QProgressBar *m_cpuBar;
    QProgressBar *m_memBar;
    QLabel *m_netLabel;
    QLabel *m_msgLabel;
    QTimer *m_timer;
    int m_tick;
};

#endif
