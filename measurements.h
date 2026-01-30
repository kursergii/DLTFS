#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include <QThread>
#include <QDebug>
#include "gpib/hp4291analyzer.h"
#include "gpib/hp8114apulser.h"
#include <QSerialPort>
#include <cstdio>

class Measurements : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        qDebug() << "Measurements thread started";

        bool allConnected = true;

        // Create serial port instance
        serialPort = new QSerialPort();
        if (!connectArduino()) {
            qWarning() << "Arduino connection failed";
            allConnected = false;
        }

        // Create GPIB device instances with specialized classes
        pulser = new HP8114APulser(0, 14);    // HP8114A Pulser at address 14
        if (!connectPulser()) {
            qWarning() << "HP8114A Pulser connection failed";
            allConnected = false;
        }

        analyzer = new HP4291Analyzer(0, 17); // HP4291A Analyzer at address 17
        if (!connectAnalyzer()) {
            qWarning() << "HP4291A Analyzer connection failed";
            allConnected = false;
        }

        // Emit connection status
        emit connected(allConnected);

        if (!allConnected) {
            qWarning() << "Not all devices connected - some functionality may be limited";
        }else {
            isReadyForInitialMeasurement = true;
            qDebug() << "All devices connected successfully";
        }

        // Main loop - check for stop condition
        while (!isInterruptionRequested()) {
            running();
        }

        qDebug() << "Measurements thread stopped";
    }

//     void MW::onConnectButtonClicked()
// {
//     // Start GPIB polling timer if devices are connected
//     if (pulser->isConnected() || analyzer->isConnected()) {
//         gpibPollTimer->start();
//         qDebug() << "Started GPIB polling timer (100ms interval)";
//     }
// }

signals:
    // void progressUpdate(int currentPoint, int totalPoints, double time, double capacitance);
    // void measurementComplete(QList<double> xData, QList<double> yData);
    // void measurementError(QString error);
    void connected(const bool& status);
    void resultsReceived();
    void sendData(const double&, const double&);
    void isDone(const bool& done);

public slots:
    void startMeasurement(const int & tP, const double & ti) {
        totalPoints = tP;
        tint = ti;
        isMeasuring = true;
    }
    void got_ui_data() {
        // Placeholder for receiving UI data if needed
    }

    void updatePulseParams(double offset, double amplitude, double duration);
    void updateMeasurementParams(int numPoints, double integrationTime);
    void updateAnalyzerFrequency(double frequencyMHz);

private slots:

private:
    HP8114APulser *pulser;        // HP8114A Pulse Generator at GPIB address 14
    HP4291Analyzer *analyzer;     // HP4291A Impedance Analyzer at GPIB address 17
    QSerialPort *serialPort;      // Arduino

    QString serialBuffer;
    QString pulserBuffer;
    QString analyzerBuffer;
    
    int totalPoints;
    int maxPointsPerMeas;
    double tint;
    bool isMeasuring = false;
    bool isReadyForInitialMeasurement = false;

    // Pulse parameters
    double pulseOffset = 0.0;      // Default 0V offset
    double pulseAmplitude = 1.0;   // Default 1V amplitude
    double pulseDuration = 100.0;  // Default 100us duration

    // bool setUpAnalyzerForMeasurement();
    bool connectPulser();
    bool connectAnalyzer();
    bool connectArduino();
    void running();
    void waiter(int time);
    void applyPulseSettings();
    void doMeasurement();
};

#endif // MEASUREMENTS_H
