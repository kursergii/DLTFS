#include "measurements.h"
#include <QTimer>
#include <QEventLoop>
#include <QElapsedTimer>

double Measurements::readCapacitance()
{
    if (!analyzer || !analyzer->isConnected()) return 0.0;

    if (!analyzer->write("*CLS;*OPC?\n")) return 0.0;
    analyzer->read();

    if (!analyzer->write("*TRG\n")) return 0.0;

    if (!analyzer->write("TRAC:VAL? DTR,1\n")) return 0.0;

    QString response = analyzer->read(1024);
    if (response.isEmpty()) return 0.0;

    int commaPos = response.indexOf(',');
    bool ok;
    double capacitance;
    if (commaPos > 0) {
        capacitance = response.left(commaPos).toDouble(&ok);
    } else {
        capacitance = response.trimmed().toDouble(&ok);
    }
    return ok ? capacitance : 0.0;
}

void Measurements::running() {
    // Handle flags first (bias update, frequency update)
    if (isBiasUpdateRequested) {
        applyBiasSettings();
        isBiasUpdateRequested = false;
    }
    if (isFrequencyUpdateRequested) {
        if (analyzer && analyzer->isConnected()) {
            if (analyzer->setupForMeasurement(pendingFrequencyHz)) {
                qDebug() << "Analyzer frequency updated to" << (pendingFrequencyHz / 1e6) << "MHz";
            } else {
                qWarning() << "Failed to update analyzer frequency";
            }
        }
        isFrequencyUpdateRequested = false;
    }
    if (isReadyForInitialMeasurement) {
        applyBiasSettings();
        isReadyForInitialMeasurement = false;
    }

    // If "Start" was pressed, run the measurement sequence
    if (isMeasuring) {
        doMeasurement();
        emit isDone(true);
        isMeasuring = false;
        return;
    }

    // Otherwise: continuous live streaming
    if (devicesReady) {
        double cap = readCapacitance();
        double cur = smu->readCurrent();
        emit sendLiveData(cap, biasVoltage, cur);
    } else {
        waiter(100);
    }
}

void Measurements::waiter(int time) {
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect(this,  SIGNAL(resultsReceived()), &loop, SLOT(quit()) );
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(time);
    loop.exec();
}

bool Measurements::connectSMU() {
    if (!smu->connect()) {
        qDebug() << "Failed to connect to K236:" << smu->getLastError();
        return false;
    }

    qDebug() << "K236: GPIB connection established";

    if (!smu->initialize()) {
        qDebug() << "Failed to initialize K236";
        return false;
    }

    if (!smu->setVoltageSource(biasVoltage, 0.1)) {
        qDebug() << "Failed to set K236 voltage source";
        return false;
    }

    smu->setZeroDuration(zeroDuration);

    if (!smu->outputOn()) {
        qDebug() << "Failed to enable K236 output";
        return false;
    }

    qDebug() << "K236: Initialized - bias" << biasVoltage << "V, zero duration" << zeroDuration << "s";
    return true;
}

bool Measurements::connectAnalyzer() {
    if (!analyzer->connect()) {
        qWarning() << "HP4291A: Failed to connect -" << analyzer->getLastError();
        return false;
    }
    qDebug() << "HP4291A: GPIB connection established";

    // Send device clear to reset GPIB interface
    analyzer->clearDevice();
    QThread::msleep(200);

    if (!analyzer->write("*RST\n")) {
        qWarning() << "HP4291A: Failed to reset instrument";
        return false;
    }
    QThread::msleep(500);

    if (!analyzer->write("*CLS\n")) {
        qWarning() << "HP4291A: Failed to clear status";
        return false;
    }

    double initialFrequency = 1000e6;
    if (!analyzer->setupForMeasurement(initialFrequency)) {
        qWarning() << "HP4291A: Failed to setup measurement frequency";
        return false;
    }
    qDebug() << "HP4291A: Measurement frequency set to" << (initialFrequency / 1e6) << "MHz";

    if (!analyzer->disableMathFunctions()) {
        qWarning() << "HP4291A: Failed to disable math functions";
        return false;
    }
    qDebug() << "HP4291A: Math functions disabled - raw data mode";

    if (!analyzer->setMeasurementFormat(HP4291Analyzer::CP)) {
        qWarning() << "HP4291A: Failed to set measurement format to CP";
        return false;
    }
    qDebug() << "HP4291A: Measurement format set to CP (Capacitance-Dissipation)";

    if (!analyzer->setupTrigger(false)) {
        qWarning() << "HP4291A: Failed to setup BUS trigger";
        return false;
    }

    if (!analyzer->write("DISP:TRAC18:CLE\n")) {
        qWarning() << "HP4291A: Failed to clear status for batch";
    }
    qDebug() << "HP4291A: BUS trigger mode enabled";

    qDebug() << "HP4291A: Initialization complete - ready for measurements";
    return true;
}


void Measurements::updateBiasParams(double biasV, double zeroDurationMs) {
    biasVoltage = biasV;
    zeroDuration = zeroDurationMs / 1000.0;
    qDebug() << "Bias params updated: Bias=" << biasV << "V, Zero duration=" << zeroDurationMs << "ms";
    isBiasUpdateRequested = true;
}

void Measurements::updateMeasurementParams(int numPoints, double integrationTime) {
    totalPoints = numPoints;
    tint = integrationTime;
    qDebug() << "Measurement params updated: Points=" << numPoints << ", Integration time=" << integrationTime << "s";
}

void Measurements::updateAnalyzerFrequency(double frequencyMHz) {
    pendingFrequencyHz = frequencyMHz * 1e6;
    isFrequencyUpdateRequested = true;
    qDebug() << "Analyzer frequency update requested:" << frequencyMHz << "MHz";
}

void Measurements::applyBiasSettings() {
    if (!smu || !smu->isConnected()) {
        qDebug() << "Cannot apply bias settings: K236 not connected";
        return;
    }

    if (smu->setSourceVoltage(biasVoltage)) {
        qDebug() << "K236: Bias voltage set to" << biasVoltage << "V";
    } else {
        qDebug() << "K236: Failed to set bias voltage";
    }

    smu->setZeroDuration(zeroDuration);
    qDebug() << "K236: Zero duration set to" << zeroDuration << "s";
}


void Measurements::doMeasurement()
{
    qDebug() << "Starting DLTFS measurement sequence...";
    qDebug() << "Parameters: Points=" << totalPoints << ", Time step=" << tint << "s";
    qDebug() << "Bias=" << biasVoltage << "V, Zero duration=" << zeroDuration << "s";

    if (!analyzer || !analyzer->isConnected()) {
        qWarning() << "HP4291A: Analyzer not connected";
        return;
    }
    if (!smu || !smu->isConnected()) {
        qWarning() << "K236: SMU not connected";
        return;
    }

    // Clear analyzer
    analyzer->write("DISP:TRAC18:CLE\n");
    QThread::msleep(100);
    analyzer->write("*CLS\n");

    QElapsedTimer globalTimer;
    globalTimer.start();

    QElapsedTimer intervalTimer;
    intervalTimer.start();

    // === Phase 1: 1 second of data at DC bias (pre-pulse, negative time) ===
    const int PRE_PULSE_MS = 1000;
    qDebug() << "Phase 1: Collecting 1s pre-pulse baseline at DC bias...";

    while (globalTimer.elapsed() < PRE_PULSE_MS) {
        if (isInterruptionRequested()) return;

        double cap = readCapacitance();
        double cur = smu->readCurrent();
        // Time relative to pulse moment: negative = before pulse
        double t = (globalTimer.elapsed() - PRE_PULSE_MS) / 1000.0;

        emit sendData(t, cap, biasVoltage, cur);

        // Maintain timing
        qint64 elapsed = intervalTimer.elapsed();
        qint64 target = static_cast<qint64>(tint * 1000);
        if (target - elapsed > 0) {
            QThread::msleep(target - elapsed);
        }
        intervalTimer.restart();
    }

    qDebug() << "Phase 1 complete.";

    // === Phase 2: Zero-bias pulse ===
    qDebug() << "Phase 2: Zero-bias pulse for" << zeroDuration << "s...";

    // Emit marker at pulse start (t=0, V=0)
    double lastCap = readCapacitance();
    emit sendData(0.0, lastCap, 0.0, 0.0);

    // Drop to 0V
    smu->sendCommand("B0.0,0,0X");

    // Continue reading during zero-bias period
    QElapsedTimer pulseTimer;
    pulseTimer.start();
    int zeroDurationMs = static_cast<int>(zeroDuration * 1000);

    while (pulseTimer.elapsed() < zeroDurationMs) {
        if (isInterruptionRequested()) return;

        double cap = readCapacitance();
        double cur = smu->readCurrent();
        double t = pulseTimer.elapsed() / 1000.0;  // time during pulse

        emit sendData(t, cap, 0.0, cur);

        qint64 elapsed = intervalTimer.elapsed();
        qint64 target = static_cast<qint64>(tint * 1000);
        if (target - elapsed > 0) {
            QThread::msleep(target - elapsed);
        }
        intervalTimer.restart();
    }

    // Restore DC bias
    smu->setSourceVoltage(biasVoltage);
    qDebug() << "Phase 2 complete. Bias restored.";

    // === Phase 3: Post-pulse transient (totalPoints at DC bias) ===
    qDebug() << "Phase 3: Collecting" << totalPoints << "post-pulse points...";

    // Reset timer for post-pulse time axis
    QElapsedTimer postTimer;
    postTimer.start();
    intervalTimer.restart();

    double pulseEndTime = zeroDuration;  // t offset where bias was restored

    for (int i = 0; i < totalPoints; ++i) {
        if (isInterruptionRequested()) {
            qWarning() << "Measurement stopped at point" << i;
            return;
        }

        double cap = readCapacitance();
        double cur = smu->readCurrent();
        double t = pulseEndTime + postTimer.elapsed() / 1000.0;

        emit sendData(t, cap, biasVoltage, cur);

        if ((i + 1) % 50 == 0) {
            qDebug() << "Point" << (i + 1) << "/" << totalPoints
                     << "t=" << t << "s, Cp=" << cap;
        }

        // Maintain timing
        qint64 elapsed = intervalTimer.elapsed();
        qint64 target = static_cast<qint64>(tint * 1000);
        if (target - elapsed > 0) {
            QThread::msleep(target - elapsed);
        }
        intervalTimer.restart();
    }

    qDebug() << "Measurement complete. Total time:" << (globalTimer.elapsed() / 1000.0) << "s";
}
