#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include <QThread>
#include <QDebug>
#include <QVector>
#include "gpib/gpibdevice.h"
#include "gpib/hp4291analyzer.h"
#include "gpib/hp8114apulser.h"
#include <QSerialPort>
#include <cstdio>

// // Forward declarations
// class QSerialPort;
// class HP8114APulser;
// class HP4291Analyzer;

class Measurements : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        
        //Create serial port instance
        serialPort = new QSerialPort();
        connectArduino();
        // Create GPIB device instances with specialized classes
        pulser = new HP8114APulser(0, 14);    // HP8114A Pulser at address 14
        connectPulser();

        analyzer = new HP4291Analyzer(0, 17); // HP4291A Analyzer at address 17
        connectAnalyzer();
        emit connected(true);
        
        while(true)
            running();
    }

public:

//     void MW::onConnectButtonClicked()
// {
//     // Start GPIB polling timer if devices are connected
//     if (pulser->isConnected() || analyzer->isConnected()) {
//         gpibPollTimer->start();
//         qDebug() << "Started GPIB polling timer (100ms interval)";
//     }
// }

signals:
    void progressUpdate(int currentPoint, int totalPoints, double time, double capacitance);
    void measurementComplete(QVector<double> xData, QVector<double> yData);
    void measurementError(QString error);
    void connected(bool status);

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
    bool isMeasuring;

    bool setUpAnalyzerForMeasurement();
    void connectPulser();
    void connectAnalyzer();
    void connectArduino();
    void running();
};

#endif // MEASUREMENTS_H
