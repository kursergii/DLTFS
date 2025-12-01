#include "mw.h"
#include "ui_mw.h"
#include <QDebug>
#include <QPushButton>
#include <QLineEdit>
#include <QApplication>
#include <QSerialPortInfo>
#include <QThread>
#include <QElapsedTimer>
#include <QVector>
#include <cmath>
#include <cstdio>

MW::MW(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MW)
    , customPlot(nullptr)
{
    meas = new Measurements;
    ui->setupUi(this);

    customPlot = ui->customPlotWidget;
    setupPlot();

    connectSignals();
}

MW::~MW()
{
    // // Clean up measurement thread if running
    // if (measurementThread && measurementThread->isRunning()) {
    //     measurementThread->stopMeasurement();
    //     measurementThread->wait();
    // }
    // delete measurementThread;

    // if (serialPort && serialPort->isOpen()) {
    //     serialPort->close();
    // }
    // delete pulser;
    // delete analyzer;
    // delete serialPort;
    delete ui;
}

void MW::connectSignals()
{
    // Connect button slots
    QObject::connect(ui->connectButton, &QPushButton::clicked, this, &MW::onConnectButtonClicked);
    QObject::connect(ui->startButton, &QPushButton::clicked, this, &MW::onStartButtonClicked);
    QObject::connect(ui->quitButton, &QPushButton::clicked, this, &MW::onQuitButtonClicked);

    // Connect pulse parameter change signals
    QObject::connect(ui->offsetDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     this, &MW::onOffsetChanged);
    QObject::connect(ui->amplitudeDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     this, &MW::onAmplitudeChanged);
    QObject::connect(ui->durationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &MW::onDurationChanged);

    // QObject::connect(serialPort, &QSerialPort::readyRead, this, &MW::onSerialDataReceived);

}

void MW::onConnectButtonClicked()
{
    qDebug() << "Connecting to devices...";

    qDebug() << "Starting measurement sequence in thread...";
    connect(meas, &Measurements::sendData, this, &MW::receive_data_meas);
    connect(meas, &Measurements::connected, this, &MW::receive_connected);
    connect(this, &MW::init_meas, meas, &Measurements::startMeasurement);
    connect(this, &MW::send_ui, meas, &Measurements::got_ui_data);
    connect(this, &MW::pulseParamsChanged, meas, &Measurements::updatePulseParams);
    connect(meas, &Measurements::finished, meas, &QObject::deleteLater);
    meas->start();

    // Send initial pulse parameters to the thread
    double offset = ui->offsetDoubleSpinBox->value();
    double amplitude = ui->amplitudeDoubleSpinBox->value();
    double duration = ui->durationSpinBox->value();
    emit pulseParamsChanged(offset, amplitude, duration);

}

void MW::receive_data_meas(const int & a, const double & b){
    updatePlot(xData, yData);
}

void MW::receive_connected(const bool& con){
    ui->startButton->setEnabled(con);
}

void MW::onStartButtonClicked()
{
    qDebug() << "Starting measurement sequence...";
    ui->startButton->setEnabled(false);

    // Get measurement parameters from UI
    int totalPoints = ui->numMeasSpinBox->value();
    double timeStep = ui->spinBox->value() / 1000.0;  // Convert ms to seconds

    emit init_meas(totalPoints, timeStep);
}


void MW::onQuitButtonClicked()
{
    qDebug() << "Quitting application...";
    QApplication::quit();
}

void MW::onPulserCommandEntered()
{
    // TODO: Implement pulser command handling
    qDebug() << "Pulser command entered";
}

void MW::onAnalyzerCommandEntered()
{
    // TODO: Implement analyzer command handling
    qDebug() << "Analyzer command entered";
}

void MW::onSerialDataReceived()
{
    // TODO: Implement serial data reception
    qDebug() << "Serial data received";
}

void MW::onArduinoTRIGButtonClicked()
{
    // TODO: Implement Arduino trigger button handling
    qDebug() << "Arduino TRIG button clicked";
}

void MW::onOffsetChanged()
{
    // Send all pulse parameters when offset changes
    double offset = ui->offsetDoubleSpinBox->value();
    double amplitude = ui->amplitudeDoubleSpinBox->value();
    double duration = ui->durationSpinBox->value();

    emit pulseParamsChanged(offset, amplitude, duration);
}

void MW::onAmplitudeChanged()
{
    // Send all pulse parameters when amplitude changes
    double offset = ui->offsetDoubleSpinBox->value();
    double amplitude = ui->amplitudeDoubleSpinBox->value();
    double duration = ui->durationSpinBox->value();

    emit pulseParamsChanged(offset, amplitude, duration);
}

void MW::onDurationChanged()
{
    // Send all pulse parameters when duration changes
    double offset = ui->offsetDoubleSpinBox->value();
    double amplitude = ui->amplitudeDoubleSpinBox->value();
    double duration = ui->durationSpinBox->value();

    emit pulseParamsChanged(offset, amplitude, duration);
}

void MW::onPulserDataReceived()
{
    // TODO: Implement pulser data reception
    qDebug() << "Pulser data received";
}

void MW::onAnalyzerDataReceived()
{
    // TODO: Implement analyzer data reception
    qDebug() << "Analyzer data received";
}

void MW::setupPlot()
{
    // Set the plot as the central widget (or you can embed it in the UI)
    // For now, let's not make it central widget to keep existing UI
    // Instead, we'll need to add it to the layout later
    
    // Configure plot appearance
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    // Add a graph
    customPlot->addGraph();
    customPlot->graph(0)->setPen(QPen(Qt::blue, 2));
    customPlot->graph(0)->setScatterStyle(QCPScatterStyle::ssCircle);

    // Set axis labels
    customPlot->xAxis->setLabel("Time (s)");
    customPlot->yAxis->setLabel("Capacitance (pF)");

    // Set up legend
    customPlot->legend->setVisible(true);
    customPlot->graph(0)->setName("Cp Measurement");

    // Enable grid
    customPlot->xAxis->grid()->setVisible(true);
    customPlot->yAxis->grid()->setVisible(true);

    // Set minimum size
    // customPlot->setMinimumSize(600, 400);

    qDebug() << "QCustomPlot initialized";
}

void MW::updatePlot(const QVector<double>& xData, const QVector<double>& yData)
{
    if (!customPlot || xData.isEmpty() || yData.isEmpty()) {
        return;
    }

    // Convert yData from F to pF for display
    QVector<double> yDataPF(yData.size());
    for (int i = 0; i < yData.size(); ++i) {
        yDataPF[i] = yData[i] * 1e12;  // Convert to pF
    }

    // Set data
    customPlot->graph(0)->setData(xData, yDataPF);

    // Auto-scale axes to fit data
    customPlot->rescaleAxes();

    // Add some margin
    customPlot->xAxis->scaleRange(1.1, customPlot->xAxis->range().center());
    customPlot->yAxis->scaleRange(1.1, customPlot->yAxis->range().center());

    // Refresh the plot
    customPlot->replot();

    qDebug() << "Plot updated with" << xData.size() << "points";
}
