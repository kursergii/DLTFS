#ifndef PICOSCOPE_H
#define PICOSCOPE_H

#include <QThread>
#include <QVector>
#include <QDebug>
#include "include/libps4000a/ps4000aApi.h"

class PicoScope : public QThread
{
    Q_OBJECT

    void run() override {
        createBuffers();
        connectToScope();

        if (handle > 0) {
            emit connectionStatus(true);
            qDebug() << "PicoScope connected, starting temperature acquisition";

            while (!isInterruptionRequested()) {
                double tempSum = 0;
                int steps = 200;
                for (int i = 0; i < steps; ++i) {
                    runCapture();
                    tempSum += retData[3];  // Channel 3 = temperature
                }
                double rawVoltage = tempSum / steps;

                // Calibration: T(K) = A * V + B
                double temperatureK = calibA * rawVoltage + calibB;
                emit temperatureReady(temperatureK);
            }
        } else {
            qWarning() << "PicoScope not connected";
            emit connectionStatus(false);
        }

        qDebug() << "PicoScope thread stopped";
    }

signals:
    void temperatureReady(double temperatureK);
    void connectionStatus(bool connected);

private:
    void connectToScope();
    void runCapture();
    void findValues();
    void checkRanges();
    void createBuffers();

    // Calibration constants
    static constexpr double calibA = 65077.49;
    static constexpr double calibB = -48.16;  // -38.16 - 10

    // PicoScope state
    int16_t handle = 0;
    uint32_t noOfSamples = 1000;
    int32_t nCaptures = 32;

    QVector<int16_t> range = {6, 6, 8, 6};
    QVector<double> maxInRange = {3, 3, 3, 3};
    QVector<double> retData = {0, 0, 0, 0};
    int16_t buffer[4][32][1000];
    QVector<QVector<QVector<double>>> DATA;

    ps4000aBlockReady lpReady;
    int32_t timeIndisposed;
    int32_t nMaxSamples;
    int16_t overflow;
    int16_t ready;
    int8_t serial;
    bool pParameter = false;
};

#endif // PICOSCOPE_H
