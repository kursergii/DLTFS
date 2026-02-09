#include "mw.h"
#include "ui_mw.h"
#include <QDebug>
#include <QPushButton>
#include <QLineEdit>
#include <QApplication>
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
    voltagePlot = ui->voltagePlotWidget;

    // Create Save Data button programmatically
    saveDataButton = new QPushButton("Save Data", this);
    saveDataButton->setGeometry(10, 470, 221, 51);
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
    voltageData.clear();
    currentData.clear();
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
    delete ui;
}

void MW::connectSignals()
{
    // Connect button slots
    QObject::connect(ui->connectButton, &QPushButton::clicked, this, &MW::onConnectButtonClicked);
    QObject::connect(ui->startButton, &QPushButton::clicked, this, &MW::onStartButtonClicked);
    QObject::connect(saveDataButton, &QPushButton::clicked, this, &MW::onSaveDataButtonClicked);
    QObject::connect(ui->quitButton, &QPushButton::clicked, this, &MW::onQuitButtonClicked);

    // Connect bias parameter change signals
    QObject::connect(ui->offsetDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     this, &MW::onBiasChanged);
    QObject::connect(ui->durationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &MW::onZeroDurationChanged);

    // Connect measurement parameter change signals
    QObject::connect(ui->numMeasSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &MW::onNumPointsChanged);
    QObject::connect(ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &MW::onTintChanged);

    // Connect set voltage button
    QObject::connect(ui->setVoltageButton, &QPushButton::clicked, this, &MW::onSetVoltageClicked);

    // Connect analyzer parameter change signals
    QObject::connect(ui->frequencyDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     this, &MW::onFrequencyChanged);

}

void MW::onConnectButtonClicked()
{
    qDebug() << "Connecting to devices...";

    qDebug() << "Starting measurement sequence in thread...";
    connect(meas, &Measurements::connected, this, &MW::receive_connected);
    connect(meas, &Measurements::finished, meas, &QObject::deleteLater);
    meas->start();

}

void MW::receive_data_meas(const double & a, const double & b, const double & v, const double & c){
    xData.append(a);
    yData.append(b);
    voltageData.append(v);
    currentData.append(c);

    // Also add to measurementData struct if we have an active temperature measurement
    if (temperatureIndex >= 0 && temperatureIndex < measurementData.temperatureData.size()) {
        measurementData.appendMeasurementPoint(a, a, b, v, c);
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
    ui->setVoltageButton->setEnabled(con);
    if (con) {
        qDebug() << "Devices connected successfully.";
        connect(this, &MW::init_meas, meas, &Measurements::startMeasurement);
        connect(this, &MW::send_ui, meas, &Measurements::got_ui_data);
        connect(this, &MW::biasParamsChanged, meas, &Measurements::updateBiasParams);
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
    voltageData.clear();
    currentData.clear();
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
        out << "# Time(s)\tCapacitance(F)\tVoltage(V)\tCurrent(A)\n";

        // Write data for this temperature
        for (int j = 0; j < measurementData.xData[i].size(); ++j) {
            double time = measurementData.xData[i][j];
            double capacitance = measurementData.yData[i][j];
            double voltage = (j < measurementData.voltageData[i].size()) ? measurementData.voltageData[i][j] : 0.0;
            double current = (j < measurementData.currentData[i].size()) ? measurementData.currentData[i][j] : 0.0;

            out << QString::number(time, 'e', 6) << "\t"
                << QString::number(capacitance, 'e', 12) << "\t"
                << QString::number(voltage, 'e', 6) << "\t"
                << QString::number(current, 'e', 6) << "\n";
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


void MW::onBiasChanged()
{
    double biasV = ui->offsetDoubleSpinBox->value();
    double zeroDurationMs = ui->durationSpinBox->value();

    emit biasParamsChanged(biasV, zeroDurationMs);
}

void MW::onZeroDurationChanged()
{
    double biasV = ui->offsetDoubleSpinBox->value();
    double zeroDurationMs = ui->durationSpinBox->value();

    emit biasParamsChanged(biasV, zeroDurationMs);
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

void MW::onSetVoltageClicked()
{
    double biasV = ui->offsetDoubleSpinBox->value();
    double zeroDurationMs = ui->durationSpinBox->value();

    emit biasParamsChanged(biasV, zeroDurationMs);
    qDebug() << "Set voltage:" << biasV << "V";
}

void MW::setupPlot()
{
    // Configure capacitance plot
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    customPlot->xAxis->setLabel("Time (s)");
    customPlot->yAxis->setLabel("Capacitance (pF)");
    customPlot->legend->setVisible(true);
    customPlot->xAxis->grid()->setVisible(true);
    customPlot->yAxis->grid()->setVisible(true);

    // Configure voltage/current plot
    voltagePlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    voltagePlot->xAxis->setLabel("Time (s)");
    voltagePlot->yAxis->setLabel("Voltage (V)");
    voltagePlot->xAxis->grid()->setVisible(true);
    voltagePlot->yAxis->grid()->setVisible(true);
    voltagePlot->legend->setVisible(true);

    // Right y-axis for current
    voltagePlot->yAxis2->setVisible(true);
    voltagePlot->yAxis2->setLabel("Current (A)");
    voltagePlot->yAxis2->setLabelColor(Qt::red);
    voltagePlot->yAxis2->setTickLabelColor(Qt::red);

    // Graph 0: Voltage (left axis)
    voltagePlot->addGraph();
    voltagePlot->graph(0)->setPen(QPen(Qt::darkGray, 2));
    voltagePlot->graph(0)->setName("Voltage");

    // Graph 1: Current (right axis)
    voltagePlot->addGraph(voltagePlot->xAxis, voltagePlot->yAxis2);
    voltagePlot->graph(1)->setPen(QPen(Qt::red, 2));
    voltagePlot->graph(1)->setName("Current");

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

    int numTemps = measurementData.temperatureData.size();

    // Ensure we have enough graphs for all temperatures
    while (customPlot->graphCount() < numTemps) {
        customPlot->addGraph();
    }

    // Update each temperature's capacitance graph
    for (int i = 0; i < numTemps; ++i) {
        if (i < measurementData.xData.size() && !measurementData.xData[i].isEmpty()) {
            // Convert yData from F to pF for display
            QVector<double> xVec = measurementData.xData[i].toVector();
            QVector<double> yVec(measurementData.yData[i].size());
            for (int j = 0; j < measurementData.yData[i].size(); ++j) {
                yVec[j] = measurementData.yData[i][j] * 1e12;  // Convert to pF
            }

            customPlot->graph(i)->setData(xVec, yVec);
            customPlot->graph(i)->setPen(QPen(colors[i % colors.size()], 2));
            customPlot->graph(i)->setScatterStyle(QCPScatterStyle::ssCircle);
            customPlot->graph(i)->setName(QString("T = %1 K").arg(measurementData.temperatureData[i], 0, 'f', 1));
        }
    }

    // Auto-scale capacitance plot
    customPlot->rescaleAxes();
    customPlot->xAxis->scaleRange(1.1, customPlot->xAxis->range().center());
    customPlot->yAxis->scaleRange(1.1, customPlot->yAxis->range().center());
    customPlot->replot();

    // Update voltage and current plots with the latest measurement data
    if (voltagePlot && numTemps > 0 && !measurementData.voltageData.isEmpty() &&
        !measurementData.voltageData.last().isEmpty()) {
        QVector<double> xVec = measurementData.xData.last().toVector();

        // Voltage graph
        QVector<double> vVec = measurementData.voltageData.last().toVector();
        voltagePlot->graph(0)->setData(xVec, vVec);

        // Current graph
        if (!measurementData.currentData.isEmpty() &&
            !measurementData.currentData.last().isEmpty()) {
            QVector<double> cVec = measurementData.currentData.last().toVector();
            voltagePlot->graph(1)->setData(xVec, cVec);
        }

        voltagePlot->rescaleAxes();
        voltagePlot->xAxis->scaleRange(1.1, voltagePlot->xAxis->range().center());
        voltagePlot->yAxis->scaleRange(1.3, voltagePlot->yAxis->range().center());
        voltagePlot->yAxis2->scaleRange(1.3, voltagePlot->yAxis2->range().center());
        voltagePlot->replot();
    }

    qDebug() << "Plot updated with" << numTemps << "temperature curves + voltage";
}

void MW::measIsDone(const bool & done){
    if (done) {
        ui->startButton->setEnabled(true);

        // Final update to show complete measurement
        updateAllTemperaturePlots();

        // Clear temporary data (measurementData struct retains all data)
        xData.clear();
        yData.clear();
        voltageData.clear();
        currentData.clear();
        pendingPlotUpdate = false;

        qDebug() << "Measurement sequence completed for temperature:"
                 << currentTemperature << "K";
        qDebug() << "Total temperature measurements:"
                 << measurementData.temperatureData.size();
    }
}