#include "logic_view_plugin.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

LogicViewPlugin::LogicViewPlugin()
    : m_statusLabel(0)
{
}

QString LogicViewPlugin::pluginName() const
{
    return "Logic Analyzer";
}

QWidget *LogicViewPlugin::createWidget(QWidget *parent)
{
    QWidget *root = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(root);

    QGroupBox *cfgBox = new QGroupBox("Capture Config", root);
    QFormLayout *form = new QFormLayout(cfgBox);
    QLineEdit *sampleRateEdit = new QLineEdit("100000000", cfgBox);
    QLineEdit *triggerChannelEdit = new QLineEdit("0", cfgBox);
    QLineEdit *triggerModeEdit = new QLineEdit("edge_rising", cfgBox);
    form->addRow("Max sample rate (Hz)", sampleRateEdit);
    form->addRow("Trigger channel", triggerChannelEdit);
    form->addRow("Trigger mode", triggerModeEdit);
    cfgBox->setLayout(form);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *armBtn = new QPushButton("Arm Trigger", root);
    QPushButton *pulseViewBtn = new QPushButton("Open PulseView Hint", root);
    btnLayout->addWidget(armBtn);
    btnLayout->addWidget(pulseViewBtn);

    m_statusLabel = new QLabel("Status: idle", root);
    m_statusLabel->setWordWrap(true);

    layout->addWidget(cfgBox);
    layout->addLayout(btnLayout);
    layout->addWidget(new QLabel("8-channel logic analyzer bridge: SUMP-compatible", root));
    layout->addWidget(m_statusLabel);
    layout->addStretch(1);

    QObject::connect(armBtn, &QPushButton::clicked, [this]() {
        if (m_statusLabel) {
            m_statusLabel->setText("Status: armed (waiting for trigger)");
        }
    });
    QObject::connect(pulseViewBtn, &QPushButton::clicked, [this]() {
        if (m_statusLabel) {
            m_statusLabel->setText("PulseView: connect to tcp://<board-ip>:9527 with SUMP driver");
        }
    });

    return root;
}

void LogicViewPlugin::onMessage(const QString &message)
{
    if (m_statusLabel) {
        m_statusLabel->setText(QString("Status: %1").arg(message));
    }
}
