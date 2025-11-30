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
    // Connect slots and signals here
    QObject::connect(ui->connectButton, &QPushButton::clicked, this, &MW::onConnectButtonClicked);
    QObject::connect(ui->startButton, &QPushButton::clicked, this, &MW::onStartButtonClicked);
    // QObject::connect(serialPort, &QSerialPort::readyRead, this, &MW::onSerialDataReceived);

}

void MW::onConnectButtonClicked()
{
    qDebug() << "Connecting to devices...";

    qDebug() << "Starting measurement sequence in thread...";
    connect(meas, &Measurements::sendData, this, &MW::receive_data_meas);
    connect(meas, &Measurements::connected, this, &MW::receive_connected);
    connect(this, &MW::init_meas, meas, &Measurements::startMeasurement);
    connect(meas, &Measurements::finished, meas, &QObject::deleteLater);
    meas->start();

}

void MW::receive_data_meas(const QVector<double>& xData, const QVector<double>& yData){
    updatePlot(xData, yData);
}

void MW::receive_connected(const bool& con){
    ui->startButton->setEnabled(con);
}

void MW::onStartButtonClicked()
{
    qDebug() << "Starting measurement sequence...";
    ui->startButton->setEnabled(false);
    emit init_meas();
}


void MW::onQuitButtonClicked()
{
    qDebug() << "Quitting application...";
    QApplication::quit();
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
