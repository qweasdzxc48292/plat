#ifndef IDEBUG_TOOL_PLUGIN_H
#define IDEBUG_TOOL_PLUGIN_H

#include <QtPlugin>
#include <QString>
#include <QWidget>

class IDebugToolPlugin
{
public:
    virtual ~IDebugToolPlugin() {}
    virtual QString pluginName() const = 0;
    virtual QWidget *createWidget(QWidget *parent = 0) = 0;
    virtual void onMessage(const QString &message) = 0;
};

#define IDebugToolPlugin_iid "com.embedded_debug_toolkit.IDebugToolPlugin/1.0"
Q_DECLARE_INTERFACE(IDebugToolPlugin, IDebugToolPlugin_iid)

#endif
