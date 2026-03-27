#include "mainwindow.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPluginLoader>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "idebug_tool_plugin.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_tabWidget(0),
      m_logView(0),
      m_reloadButton(0),
      m_heartbeatTimer(0)
{
    setupUi();
    m_pluginDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/plugins");
    loadPlugins(m_pluginDir);

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(1000);
    connect(m_heartbeatTimer, SIGNAL(timeout()), this, SLOT(emitHeartbeat()));
    m_heartbeatTimer->start();
}

MainWindow::~MainWindow()
{
    unloadPlugins();
}

void MainWindow::setupUi()
{
    QWidget *root = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(root);
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    QLabel *title = new QLabel("Embedded Linux Debug Toolkit (Qt 5.9)", root);
    m_reloadButton = new QPushButton("Reload Plugins", root);

    toolbarLayout->addWidget(title);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(m_reloadButton);
    mainLayout->addLayout(toolbarLayout);

    m_tabWidget = new QTabWidget(root);
    mainLayout->addWidget(m_tabWidget, 1);

    m_logView = new QTextEdit(root);
    m_logView->setReadOnly(true);
    m_logView->setMinimumHeight(140);
    mainLayout->addWidget(m_logView);

    setCentralWidget(root);
    setWindowTitle("Debug Workbench");
    resize(1080, 760);

    connect(m_reloadButton, SIGNAL(clicked()), this, SLOT(reloadPlugins()));
}

void MainWindow::appendLog(const QString &message)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_logView->append(QString("[%1] %2").arg(ts).arg(message));
}

void MainWindow::reloadPlugins()
{
    appendLog("Reloading plugins...");
    loadPlugins(m_pluginDir);
}

void MainWindow::loadPlugins(const QString &path)
{
    QDir dir(path);
    QStringList filters;
    filters << "*.so"
            << "*.dll"
            << "*.dylib";

    unloadPlugins();

    if (!dir.exists()) {
        appendLog(QString("Plugin directory missing: %1").arg(path));
        return;
    }

    const QStringList files = dir.entryList(filters, QDir::Files);
    for (int i = 0; i < files.size(); ++i) {
        const QString absPath = dir.absoluteFilePath(files.at(i));
        QPluginLoader *loader = new QPluginLoader(absPath, this);
        QObject *instance = loader->instance();
        if (!instance) {
            appendLog(QString("Load failed: %1 (%2)").arg(files.at(i)).arg(loader->errorString()));
            delete loader;
            continue;
        }

        IDebugToolPlugin *plugin = qobject_cast<IDebugToolPlugin *>(instance);
        if (!plugin) {
            appendLog(QString("Incompatible plugin: %1").arg(files.at(i)));
            loader->unload();
            delete loader;
            continue;
        }

        QWidget *widget = plugin->createWidget(m_tabWidget);
        if (!widget) {
            widget = new QLabel("Plugin created no widget", this);
        }
        m_tabWidget->addTab(widget, plugin->pluginName());

        LoadedPluginItem item;
        item.loader = loader;
        item.plugin = plugin;
        item.instance = instance;
        item.widget = widget;
        item.fileName = files.at(i);
        m_plugins.push_back(item);

        appendLog(QString("Loaded plugin: %1").arg(plugin->pluginName()));
    }

    if (m_plugins.isEmpty()) {
        appendLog("No plugins loaded.");
    }
}

void MainWindow::unloadPlugins()
{
    for (int i = 0; i < m_plugins.size(); ++i) {
        if (m_plugins[i].widget) {
            m_tabWidget->removeTab(m_tabWidget->indexOf(m_plugins[i].widget));
            delete m_plugins[i].widget;
            m_plugins[i].widget = 0;
        }
        if (m_plugins[i].loader) {
            if (m_plugins[i].instance) {
                delete m_plugins[i].instance;
                m_plugins[i].instance = 0;
            }
            m_plugins[i].loader->unload();
            delete m_plugins[i].loader;
            m_plugins[i].loader = 0;
        }
    }
    m_plugins.clear();
}

void MainWindow::emitHeartbeat()
{
    const QString msg = QString("heartbeat %1").arg(QDateTime::currentDateTime().toString("hh:mm:ss"));
    for (int i = 0; i < m_plugins.size(); ++i) {
        if (m_plugins[i].plugin) {
            m_plugins[i].plugin->onMessage(msg);
        }
    }
}
