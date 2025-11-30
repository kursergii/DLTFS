#ifndef MW_H
#define MW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include "measurements.h"
#include "qcustomplot.h"
#include "gpib/hp8114apulser.h"
#include "gpib/hp4291analyzer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MW;
}
QT_END_NAMESPACE

class MW : public QMainWindow
{
    Q_OBJECT

public:
    MW(QWidget *parent = nullptr);
    ~MW();

    void connectSignals();
    void populateSerialPorts();
    Measurements *meas;

public slots:
    void MW::receive_connected(const bool &);

private slots:
    void onConnectButtonClicked();
    void onStartButtonClicked();
    void onQuitButtonClicked();
    void onPulserCommandEntered();
    void onAnalyzerCommandEntered();
    void onSerialDataReceived();
    void onArduinoTRIGButtonClicked();
    void onAmplitudeChanged();
    void onDurationChanged();
    void onPulserDataReceived();
    void onAnalyzerDataReceived();

private:
    Ui::MW *ui;
    QCustomPlot *customPlot;    // Plot widget for displaying measurement data
    void setupPlot();
    void updatePlot(const QVector<double>&, const QVector<double>&);
    QVector<double> xData;
    QVector<double> yData;
    QVector<double> timeData;
    struct Measurementdata {
        QVector<double> temperatureData;
    QVector<QVector<double>> xData;
    QVector<QVector<double>> yData;
    QVector<QVector<double>> timeData;
    void clear() {
        temperatureData.clear();
        xData.clear();
        yData.clear();
        timeData.clear();}

    } measurementData;
    void appendTempData(double temp) {
        measurementData.temperatureData.append(temp);
        measurementData.xData.append(QVector<double>());
        measurementData.yData.append(QVector<double>());
        measurementData.timeData.append(QVector<double>());
    }
    void appendMeasurementPoint(double time, double x, double y) {
        if (!measurementData.xData.isEmpty()) {
            measurementData.timeData.last().append(time);
            measurementData.xData.last().append(x);
            measurementData.yData.last().append(y);
        }
    }
    
};
#endif // MW_H
