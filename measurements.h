#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include <QThread>
#include <QDebug>
#include "gpib/hp4291analyzer.h"
#include "gpib/keithley236smu.h"
#include <cstdio>

class Measurements : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        qDebug() << "Measurements thread started";

        bool allConnected = true;

        // Create Keithley 236 SMU instance (GPIB address 15)
        smu = new Keithley236SMU(0, 15);
        if (!connectSMU()) {
            qWarning() << "Keithley 236 SMU connection failed";
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
            devicesReady = true;
            qDebug() << "All devices connected successfully";
        }

        // Main loop - check for stop condition
        while (!isInterruptionRequested()) {
            running();
        }

        qDebug() << "Measurements thread stopped";
    }

signals:
    void connected(const bool& status);
    void resultsReceived(); // Signal to indicate new results are available, wait stopper.
    void sendLiveData(double capacitance, double voltage, double current);
    void sendData(const double&, const double&, const double&, const double&);
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

    void updateBiasParams(double biasV, double zeroDurationMs);
    void updateMeasurementParams(int numPoints, double integrationTime);
    void updateAnalyzerFrequency(double frequencyMHz);

private slots:

private:
    Keithley236SMU *smu;          // Keithley 236 SMU at GPIB address 15
    HP4291Analyzer *analyzer;     // HP4291A Impedance Analyzer at GPIB address 17

    QString smuBuffer;
    QString analyzerBuffer;

    int totalPoints;
    int maxPointsPerMeas;
    double tint;
    bool isMeasuring = false;
    bool isReadyForInitialMeasurement = false;
    bool isBiasUpdateRequested = false;
    bool isFrequencyUpdateRequested = false;
    double pendingFrequencyHz = 1000e6;

    // Bias parameters
    double biasVoltage = 1.0;      // Default 1V DC bias
    double zeroDuration = 0.1;     // Default 100ms zero-bias duration (seconds)

    bool connectSMU();
    bool connectAnalyzer();
    void running();
    void waiter(int time);
    void applyBiasSettings();
    void doMeasurement();
    double readCapacitance();  // Read one capacitance value from analyzer
    bool devicesReady = false;
};

#endif // MEASUREMENTS_H
