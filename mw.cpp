#include "mw.h"
#include "ui_mw.h"
#include <QDebug>
#include <QPushButton>
#include <QLineEdit>
#include <QApplication>
#include <QSerialPortInfo>
#include <QThread>
#include <QElapsedTimer>
#include <QList>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <cmath>
#include <cstdio>

MW::MW(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MW)
    , pendingPlotUpdate(false)
    , currentTemperature(300.0)  // Start at 300K
    , temperatureIndex(-1)  // No active measurement initially
{
    meas = new Measurements;
    ui->setupUi(this);
    customPlot = ui->customPlotWidget;

    // Create Save Data button programmatically
    saveDataButton = new QPushButton("Save Data", this);
    saveDataButton->setGeometry(10, 430, 221, 51);
    QFont buttonFont;
    buttonFont.setPointSize(16);
    buttonFont.setBold(true);
    saveDataButton->setFont(buttonFont);
    saveDataButton->setFlat(true);

    ui->pulserGroupBox->setFlat(true);
    ui->MeasGroupBox->setFlat(true);
    ui->analyzerGroupBox->setFlat(true);
    ui->temoGroupBox->setFlat(true);
    ui->pulserGroupBox->hide();
    ui->MeasGroupBox->hide();
    ui->analyzerGroupBox->hide();
    ui->temoGroupBox->hide();
    setupPlot();
    xData.clear();
    yData.clear();
    timeData.clear();
    measurementData.clear();

    // Setup plot update timer for batched updates (50ms = 20 updates/sec)
    plotUpdateTimer = new QTimer(this);
    plotUpdateTimer->setInterval(50);  // Update plot every 50ms
    connect(plotUpdateTimer, &QTimer::timeout, this, &MW::updatePlotBatched);

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
    QObject::connect(saveDataButton, &QPushButton::clicked, this, &MW::onSaveDataButtonClicked);
    QObject::connect(ui->quitButton, &QPushButton::clicked, this, &MW::onQuitButtonClicked);

    // Connect pulse parameter change signals
    QObject::connect(ui->offsetDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     this, &MW::onOffsetChanged);
    QObject::connect(ui->amplitudeDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     this, &MW::onAmplitudeChanged);
    QObject::connect(ui->durationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &MW::onDurationChanged);

    // Connect measurement parameter change signals
    QObject::connect(ui->numMeasSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &MW::onNumPointsChanged);
    QObject::connect(ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &MW::onTintChanged);

    // Connect analyzer parameter change signals
    QObject::connect(ui->frequencyDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     this, &MW::onFrequencyChanged);

    // QObject::connect(serialPort, &QSerialPort::readyRead, this, &MW::onSerialDataReceived);

}

void MW::onConnectButtonClicked()
{
    qDebug() << "Connecting to devices...";

    qDebug() << "Starting measurement sequence in thread...";
    connect(meas, &Measurements::connected, this, &MW::receive_connected);
    connect(meas, &Measurements::finished, meas, &QObject::deleteLater);
    meas->start();

}

void MW::receive_data_meas(const double & a, const double & b){
    xData.append(a);
    yData.append(b);

    // Also add to measurementData struct if we have an active temperature measurement
    if (temperatureIndex >= 0 && temperatureIndex < measurementData.temperatureData.size()) {
        measurementData.appendMeasurementPoint(a, a, b);
    }

    // Mark that we have pending data to plot
    pendingPlotUpdate = true;

    // Start timer if not already running
    if (!plotUpdateTimer->isActive()) {
        plotUpdateTimer->start();
    }
}

void MW::updatePlotBatched()
{
    // Only update if we have pending data
    if (!pendingPlotUpdate) {
        return;
    }

    // Update the plot with all accumulated data
    updatePlot(xData, yData);

    // Clear the pending flag
    pendingPlotUpdate = false;
}

void MW::receive_connected(const bool& con){
    ui->startButton->setEnabled(con);
    ui->connectButton->setEnabled(!con);
    ui->pulserGroupBox->setVisible(con);
    ui->MeasGroupBox->setVisible(con);
    ui->analyzerGroupBox->setVisible(con);
    ui->temoGroupBox->setVisible(con);
    if (con) {
        qDebug() << "Devices connected successfully.";
        connect(this, &MW::init_meas, meas, &Measurements::startMeasurement);
        connect(this, &MW::send_ui, meas, &Measurements::got_ui_data);
        connect(this, &MW::pulseParamsChanged, meas, &Measurements::updatePulseParams);
        connect(this, &MW::measurementParamsChanged, meas, &Measurements::updateMeasurementParams);
        connect(this, &MW::analyzerFrequencyChanged, meas, &Measurements::updateAnalyzerFrequency);
        connect(meas, &Measurements::sendData, this, &MW::receive_data_meas);
        connect(meas, &Measurements::isDone, this, &MW::measIsDone);

    } else {
        qDebug() << "Failed to connect to devices.";
    }
}

void MW::onStartButtonClicked()
{
    qDebug() << "Starting measurement sequence...";
    ui->startButton->setEnabled(false);

    // Simulate a new temperature (increment by 10K each time)
    currentTemperature += 10.0;

    // Add new temperature to measurementData
    measurementData.appendTempData(currentTemperature);
    temperatureIndex = measurementData.temperatureData.size() - 1;

    qDebug() << "Starting measurement at temperature:" << currentTemperature << "K";
    qDebug() << "Temperature measurement index:" << temperatureIndex;

    // Clear current measurement data (not the stored measurementData)
    xData.clear();
    yData.clear();
    pendingPlotUpdate = false;

    // Get measurement parameters from UI
    int totalPoints = ui->numMeasSpinBox->value();
    double timeStep = ui->spinBox->value() / 1000.0;  // Convert ms to seconds

    emit init_meas(totalPoints, timeStep);
}


void MW::onSaveDataButtonClicked()
{
    qDebug() << "Saving measurement data...";

    // Check if there's any data to save
    if (measurementData.temperatureData.isEmpty()) {
        qDebug() << "No measurement data to save";
        return;
    }

    // Generate folder name with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString folderName = QString("DLTFS_data_%1").arg(timestamp);

    // Create the directory
    QDir dir;
    if (!dir.mkpath(folderName)) {
        qWarning() << "Failed to create directory:" << folderName;
        return;
    }

    qDebug() << "Created folder:" << folderName;

    // Save each temperature in a separate text file
    for (int i = 0; i < measurementData.temperatureData.size(); ++i) {
        double temp = measurementData.temperatureData[i];

        // Generate filename for this temperature
        QString filename = QString("%1/T_%2K.txt")
                               .arg(folderName)
                               .arg(temp, 0, 'f', 1);

        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to open file for writing:" << filename;
            continue;
        }

        QTextStream out(&file);

        // Write header with metadata
        out << "# DLTFS Measurement Data\n";
        out << "# Temperature: " << QString::number(temp, 'f', 1) << " K\n";
        out << "# Saved: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
        out << "# Number of points: " << measurementData.xData[i].size() << "\n";
        out << "#\n";

        // Write column headers
        out << "# Time(s)\tCapacitance(F)\n";

        // Write data for this temperature
        for (int j = 0; j < measurementData.xData[i].size(); ++j) {
            double time = measurementData.xData[i][j];
            double capacitance = measurementData.yData[i][j];

            // Write: Time, Capacitance (tab-separated)
            out << QString::number(time, 'e', 6) << "\t"
                << QString::number(capacitance, 'e', 12) << "\n";
        }

        file.close();
        qDebug() << "Saved temperature" << temp << "K to:" << filename;
    }

    qDebug() << "All data saved successfully to folder:" << folderName;
    qDebug() << "Total files created:" << measurementData.temperatureData.size();
}

void MW::onQuitButtonClicked()
{
    qDebug() << "Quitting application...";
    QApplication::quit();
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

void MW::onNumPointsChanged()
{
    // Send measurement parameters when number of points changes
    int numPoints = ui->numMeasSpinBox->value();
    double tint = ui->spinBox->value() / 1000.0;  // Convert ms to seconds

    emit measurementParamsChanged(numPoints, tint);
    qDebug() << "Number of points changed to:" << numPoints;
}

void MW::onTintChanged()
{
    // Send measurement parameters when integration time changes
    int numPoints = ui->numMeasSpinBox->value();
    double tint = ui->spinBox->value() / 1000.0;  // Convert ms to seconds

    emit measurementParamsChanged(numPoints, tint);
    qDebug() << "Integration time changed to:" << tint << "s";
}

void MW::onFrequencyChanged()
{
    // Send analyzer frequency when changed
    double frequencyMHz = ui->frequencyDoubleSpinBox->value();

    emit analyzerFrequencyChanged(frequencyMHz);
    qDebug() << "Analyzer frequency changed to:" << frequencyMHz << "MHz";
}

void MW::setupPlot()
{
    // Configure plot appearance
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    // Set axis labels
    customPlot->xAxis->setLabel("Time (s)");
    customPlot->yAxis->setLabel("Capacitance (pF)");

    // Set up legend
    customPlot->legend->setVisible(true);

    // Enable grid
    customPlot->xAxis->grid()->setVisible(true);
    customPlot->yAxis->grid()->setVisible(true);

    qDebug() << "QCustomPlot initialized";
}

void MW::updatePlot(const QList<double>& xData, const QList<double>& yData)
{
    if (!customPlot || xData.isEmpty() || yData.isEmpty()) {
        return;
    }

    // Update all temperature plots instead of just current data
    updateAllTemperaturePlots();
}

void MW::updateAllTemperaturePlots()
{
    if (!customPlot) return;

    // Define color palette for different temperatures
    QList<QColor> colors = {Qt::blue, Qt::red, Qt::green, Qt::magenta, Qt::cyan,
                            Qt::darkYellow, Qt::darkBlue, Qt::darkRed, Qt::darkGreen, Qt::darkMagenta};

    // Ensure we have enough graphs for all temperatures
    while (customPlot->graphCount() < measurementData.temperatureData.size()) {
        customPlot->addGraph();
    }

    // Update each temperature's graph
    for (int i = 0; i < measurementData.temperatureData.size(); ++i) {
        if (i < measurementData.xData.size() && !measurementData.xData[i].isEmpty()) {
            // Convert yData from F to pF for display
            QVector<double> xVec = measurementData.xData[i].toVector();
            QVector<double> yVec(measurementData.yData[i].size());
            for (int j = 0; j < measurementData.yData[i].size(); ++j) {
                yVec[j] = measurementData.yData[i][j] * 1e12;  // Convert to pF
            }

            // Set graph data and appearance
            customPlot->graph(i)->setData(xVec, yVec);
            customPlot->graph(i)->setPen(QPen(colors[i % colors.size()], 2));
            customPlot->graph(i)->setScatterStyle(QCPScatterStyle::ssCircle);
            customPlot->graph(i)->setName(QString("T = %1 K").arg(measurementData.temperatureData[i], 0, 'f', 1));
        }
    }

    // Auto-scale axes to fit all data
    customPlot->rescaleAxes();

    // Add some margin
    customPlot->xAxis->scaleRange(1.1, customPlot->xAxis->range().center());
    customPlot->yAxis->scaleRange(1.1, customPlot->yAxis->range().center());

    // Refresh the plot
    customPlot->replot();

    qDebug() << "Plot updated with" << measurementData.temperatureData.size() << "temperature curves";
}

void MW::measIsDone(const bool & done){
    if (done) {
        ui->startButton->setEnabled(true);

        // Final update to show complete measurement
        updateAllTemperaturePlots();

        // Clear temporary data (measurementData struct retains all data)
        xData.clear();
        yData.clear();
        pendingPlotUpdate = false;

        qDebug() << "Measurement sequence completed for temperature:"
                 << currentTemperature << "K";
        qDebug() << "Total temperature measurements:"
                 << measurementData.temperatureData.size();
    }
}