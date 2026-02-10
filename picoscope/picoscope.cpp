#include "picoscope.h"
#include <cmath>
#include <algorithm>

void PicoScope::connectToScope()
{
    qDebug() << "PicoScope: Opening connection...";
    PICO_STATUS status = ps4000aOpenUnit(&handle, &serial);
    if (status != PICO_OK) {
        qWarning() << "PicoScope: Failed to open unit, status:" << status;
        return;
    }
    qDebug() << "PicoScope: Unit opened, handle:" << handle;

    // Setup all 4 channels
    for (int i = 0; i < 4; ++i) {
        status = ps4000aSetChannel(handle, static_cast<enPS4000AChannel>(i),
                                   1, PS4000A_DC,
                                   static_cast<PICO_CONNECT_PROBE_RANGE>(range[i]), 0);
        if (status != PICO_OK)
            qWarning() << "PicoScope: SetChannel" << i << "failed:" << status;
    }

    // Simple trigger on Channel A
    status = ps4000aSetSimpleTrigger(handle, 1, PS4000A_CHANNEL_A, 500, PS4000A_RISING, 0, 100);
    if (status != PICO_OK)
        qWarning() << "PicoScope: SetSimpleTrigger failed:" << status;

    // Memory segments for rapid block mode
    status = ps4000aMemorySegments(handle, 1280, &nMaxSamples);
    if (status != PICO_OK)
        qWarning() << "PicoScope: MemorySegments failed:" << status;

    status = ps4000aSetNoOfCaptures(handle, nCaptures);
    if (status != PICO_OK)
        qWarning() << "PicoScope: SetNoOfCaptures failed:" << status;

    // Set data buffers for all channels and captures
    for (int32_t i = 0; i < nCaptures; ++i) {
        for (int j = 0; j < 4; ++j) {
            status = ps4000aSetDataBuffer(handle, static_cast<enPS4000AChannel>(j),
                                          buffer[j][i], noOfSamples, i,
                                          PS4000A_RATIO_MODE_NONE);
            if (status != PICO_OK)
                qWarning() << "PicoScope: SetDataBuffer ch" << j << "seg" << i << "failed:" << status;
        }
    }

    qDebug() << "PicoScope: Setup complete";
}

void PicoScope::runCapture()
{
    ready = 0;
    PICO_STATUS status = ps4000aRunBlock(handle, noOfSamples / 2, noOfSamples / 2,
                                         2, &timeIndisposed, 0, lpReady, &pParameter);
    if (status != PICO_OK) {
        qWarning() << "PicoScope: RunBlock failed:" << status;
        return;
    }

    // Wait for data
    while (ready != 1)
        ps4000aIsReady(handle, &ready);

    status = ps4000aGetValuesBulk(handle, &noOfSamples, 0, nCaptures - 1,
                                   1, PS4000A_RATIO_MODE_NONE, &overflow);

    findValues();
    checkRanges();
}

void PicoScope::findValues()
{
    double poweR = 0, tempeR = 0;
    for (int j = 0; j < nCaptures; ++j) {
        double power = 0, temper = 0;
        for (uint32_t k = 0; k < noOfSamples; ++k) {
            DATA[0][j][k] = fabs(buffer[0][j][k]);
            DATA[1][j][k] = fabs(buffer[1][j][k]);
            if (k < noOfSamples / 2) {
                temper += fabs(buffer[3][j][k]);
                power += buffer[2][j][k];
            }
        }
        poweR += power / (noOfSamples / 2);
        tempeR += temper / (noOfSamples / 2);
    }
    maxInRange[2] = poweR / nCaptures;
    maxInRange[3] = tempeR / nCaptures;

    double current = 0, voltage = 0;
    for (int j = 0; j < nCaptures; ++j) {
        current += *std::max_element(DATA[0][j].begin(), DATA[0][j].end());
        voltage += *std::max_element(DATA[1][j].begin(), DATA[1][j].end());
    }
    maxInRange[0] = current / nCaptures;
    maxInRange[1] = voltage / nCaptures;
}

void PicoScope::checkRanges()
{
    bool newRun = false;
    for (int i = 0; i < 4; ++i) {
        if (i == 2) continue;  // Skip power channel
        if (maxInRange[i] > 30000) {
            if (range[i] != 13) {
                ++range[i];
                ps4000aSetChannel(handle, static_cast<enPS4000AChannel>(i),
                                  1, PS4000A_DC,
                                  static_cast<PICO_CONNECT_PROBE_RANGE>(range[i]), 0);
                newRun = true;
            }
        } else if (maxInRange[i] < 600) {
            if (range[i] != 2) {
                --range[i];
                ps4000aSetChannel(handle, static_cast<enPS4000AChannel>(i),
                                  1, PS4000A_DC,
                                  static_cast<PICO_CONNECT_PROBE_RANGE>(range[i]), 0);
                newRun = true;
            }
        }
    }

    if (newRun) {
        runCapture();
    } else {
        // Convert raw ADC values to physical units using range calibration
        QVector<double> rangeValues{
            0,                          // 10 mV
            0,                          // 20 mV
            1.5260651935050665E-06,     // 50 mV
            3.052130387010133E-06,      // 100 mV
            6.1042607740202665E-06,     // 200 mV
            1.5260651935050665E-05,     // 500 mV
            3.052130387010133E-05,      // 1 V
            6.1042607740202665E-05,     // 2 V
            0.00015260651935050665,     // 5 V
            0.0003052130387010133,      // 10 V
            0.00061042607740202665,     // 20 V
            0.0015260651935050665,      // 50 V
            0.003052130387010133,       // 100 V
            0.0061042607740202665,      // 200 V
        };

        for (int i = 0; i < 4; ++i)
            retData[i] = maxInRange[i] * rangeValues[range[i]];
    }
}

void PicoScope::createBuffers()
{
    DATA.resize(2);
    DATA[0].resize(nCaptures);
    DATA[1].resize(nCaptures);
    for (int j = 0; j < nCaptures; ++j) {
        DATA[0][j].resize(noOfSamples);
        DATA[1][j].resize(noOfSamples);
    }
}
