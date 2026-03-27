#include "capture_tools_plugin.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

CaptureToolsPlugin::CaptureToolsPlugin()
    : m_logView(0)
{
}

QString CaptureToolsPlugin::pluginName() const
{
    return "Capture Tools";
}

QWidget *CaptureToolsPlugin::createWidget(QWidget *parent)
{
    QWidget *root = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(root);
    QHBoxLayout *buttonRow = new QHBoxLayout();

    QPushButton *v4l2Btn = new QPushButton("Probe V4L2", root);
    QPushButton *netBtn = new QPushButton("Start Net Capture", root);
    QPushButton *sysBtn = new QPushButton("Snapshot System", root);

    buttonRow->addWidget(v4l2Btn);
    buttonRow->addWidget(netBtn);
    buttonRow->addWidget(sysBtn);

    m_logView = new QTextEdit(root);
    m_logView->setReadOnly(true);
    m_logView->append("Capture tools ready.");

    layout->addLayout(buttonRow);
    layout->addWidget(m_logView, 1);

    QObject::connect(v4l2Btn, &QPushButton::clicked, [this]() {
        if (m_logView) {
            m_logView->append("V4L2 probe requested: /dev/video0");
        }
    });
    QObject::connect(netBtn, &QPushButton::clicked, [this]() {
        if (m_logView) {
            m_logView->append("Network capture requested on eth0");
        }
    });
    QObject::connect(sysBtn, &QPushButton::clicked, [this]() {
        if (m_logView) {
            m_logView->append("System snapshot requested.");
        }
    });

    return root;
}

void CaptureToolsPlugin::onMessage(const QString &message)
{
    if (m_logView) {
        const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
        m_logView->append(QString("[%1] %2").arg(ts).arg(message));
    }
}
