#ifndef MW_H
#define MW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include "measurements.h"
#include "qcustomplot/qcustomplot.h"
#include "gpib/hp8114apulser.h"
#include "gpib/hp4291analyzer.h"
#include "QList"

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

public:
signals:
    void init_meas(const int &, const double &);
    void sendData(const int &, const double &);
    void send_ui();
    void pulseParamsChanged(double offset, double amplitude, double duration);
    void measurementParamsChanged(int numPoints, double integrationTime);
    void analyzerFrequencyChanged(double frequencyMHz);    

private slots:
    void receive_connected(const bool &);
    void receive_data_meas(const double &, const double &);
    void onConnectButtonClicked();
    void onStartButtonClicked();
    void onSaveDataButtonClicked();
    void onQuitButtonClicked();
    void onOffsetChanged();
    void onAmplitudeChanged();
    void onDurationChanged();
    void onNumPointsChanged();
    void onTintChanged();
    void onFrequencyChanged();
    void measIsDone(const bool & done);

private:
    Ui::MW *ui;
    QCustomPlot *customPlot;    // Plot widget for displaying measurement data
    QPushButton *saveDataButton;  // Save data button (created programmatically)
    void setupPlot();
    void updatePlot(const QList<double>&, const QList<double>&);
    void updatePlotBatched();  // Batched update for performance
    void updateAllTemperaturePlots();  // Update all temperature curves
    QList<double> xData;
    QList<double> yData;
    QList<double> timeData;
    QTimer *plotUpdateTimer;  // Timer for batched plot updates
    bool pendingPlotUpdate;   // Flag for pending updates
    double currentTemperature;  // Current simulated temperature
    int temperatureIndex;  // Index for current temperature measurement
    struct Measurementdata {
        QList<double> temperatureData;
        QList<QList<double>> xData;
        QList<QList<double>> yData;
        QList<QList<double>> timeData;
        void clear() {
            temperatureData.clear();
            xData.clear();
            yData.clear();
            timeData.clear();}
        void appendTempData(double temp) {
            temperatureData.append(temp);
            xData.append(QList<double>());
            yData.append(QList<double>());
            timeData.append(QList<double>());
        }
        void appendMeasurementPoint(double time, double x, double y) {
            if (!xData.isEmpty()) {
                timeData.last().append(time);
                xData.last().append(x);
                yData.last().append(y);
            }
        }
    } measurementData;
    
};
#endif // MW_H
