#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include <QThread>
#include <QTimer>
#include <QEventLoop>
#include <QDebug>
#include <QVector>
#include <QElapsedTimer>

// Forward declarations
class QSerialPort;
class HP8114APulser;
class HP4291Analyzer;

class Measurements : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        connectPulser();
        connectAnalyzer();
        connectArduino();
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

private:
    HP8114APulser *pulser;        // HP8114A Pulse Generator at GPIB address 14
    HP4291Analyzer *analyzer;     // HP4291A Impedance Analyzer at GPIB address 17
    QSerialPort *serialPort;      // Arduino
   // Arduino
    QTimer *gpibPollTimer;      // Timer for polling GPIB devices

    QString serialBuffer;
    QString pulserBuffer;
    QString analyzerBuffer;
    
    int totalPoints;
    int maxPointsPerMeas;
    double tint;
    bool shouldStop;
    bool isMeasuring;

    bool setUpAnalyzerForMeasurement();
    void connectPulser();
    void connectAnalyzer();
    void connectArduino();
    void running();
};

#endif // MEASUREMENTS_H
