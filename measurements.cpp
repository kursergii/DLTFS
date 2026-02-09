#include "measurements.h"
#include <QTimer>
#include <QEventLoop>
#include <QElapsedTimer>

void Measurements::running() {
    waiter(100);
    qDebug() << "Measurements running...";
    if (isMeasuring) {
        doMeasurement();
        emit isDone(true);
        isMeasuring = false;
    }
    else if (isReadyForInitialMeasurement) {
        applyBiasSettings();
        isReadyForInitialMeasurement = false;
    }
    else if (isBiasUpdateRequested) {
        applyBiasSettings();
        isBiasUpdateRequested = false;
    }
    else if (isFrequencyUpdateRequested) {
        if (analyzer && analyzer->isConnected()) {
            if (analyzer->setupForMeasurement(pendingFrequencyHz)) {
                qDebug() << "Analyzer frequency updated to" << (pendingFrequencyHz / 1e6) << "MHz";
            } else {
                qWarning() << "Failed to update analyzer frequency";
            }
        }
        isFrequencyUpdateRequested = false;
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
    // Connect to Keithley 236 SMU (GPIB address 16)
    if (!smu->connect()) {
        qDebug() << "Failed to connect to K236:" << smu->getLastError();
        return false;
    }

    qDebug() << "K236: GPIB connection established";

    // Initialize the SMU (includes reset and voltage source mode setup)
    if (!smu->initialize()) {
        qDebug() << "Failed to initialize K236";
        return false;
    }

    // Set initial bias voltage with compliance
    if (!smu->setVoltageSource(biasVoltage, 0.1)) {
        qDebug() << "Failed to set K236 voltage source";
        return false;
    }

    // Set zero-bias duration
    smu->setZeroDuration(zeroDuration);

    // Turn output on
    if (!smu->outputOn()) {
        qDebug() << "Failed to enable K236 output";
        return false;
    }

    qDebug() << "K236: Initialized - bias" << biasVoltage << "V, zero duration" << zeroDuration << "s";
    return true;
}

bool Measurements::connectAnalyzer() {
    // Step 1: Establish GPIB connection to HP 4291A Impedance Analyzer (address 17)
    if (!analyzer->connect()) {
        qWarning() << "HP4291A: Failed to connect -" << analyzer->getLastError();
        return false;
    }
    qDebug() << "HP4291A: GPIB connection established";

    // Step 2: Verify device identification
    QString analyzerID = analyzer->queryIdentification();
    if (analyzerID.isEmpty()) {
        qWarning() << "HP4291A: Failed to query device identification";
        return false;
    }
    qDebug() << "HP4291A: Device ID:" << analyzerID;

    if (!analyzer->write("*CLS\n")) {
        qWarning() << "HP4291A: Failed to clear status at point";
        return false;
    }

    // Step 3: Setup measurement frequency (1000 MHz for DLTFS)
    double initialFrequency = 1000e6;  // 1000 MHz
    if (!analyzer->setupForMeasurement(initialFrequency)) {
        qWarning() << "HP4291A: Failed to setup measurement frequency";
        return false;
    }
    qDebug() << "HP4291A: Measurement frequency set to" << (initialFrequency / 1e6) << "MHz";

    // Step 4: Disable math functions for raw capacitance data
    if (!analyzer->disableMathFunctions()) {
        qWarning() << "HP4291A: Failed to disable math functions";
        return false;
    }
    qDebug() << "HP4291A: Math functions disabled - raw data mode";

    // Step 5: Set measurement format to CP (Capacitance parallel + Dissipation factor)
    if (!analyzer->setMeasurementFormat(HP4291Analyzer::CP)) {
        qWarning() << "HP4291A: Failed to set measurement format to CP";
        return false;
    }
    qDebug() << "HP4291A: Measurement format set to CP (Capacitance-Dissipation)";

    // Step 6: Configure BUS trigger mode (software trigger via GPIB)
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
    zeroDuration = zeroDurationMs / 1000.0;  // Convert ms to seconds
    qDebug() << "Bias params updated: Bias=" << biasV << "V, Zero duration=" << zeroDurationMs << "ms";

    // Set flag so the worker thread applies settings via GPIB
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

    // Update bias voltage on the SMU
    if (smu->setSourceVoltage(biasVoltage)) {
        qDebug() << "K236: Bias voltage set to" << biasVoltage << "V";
    } else {
        qDebug() << "K236: Failed to set bias voltage";
    }

    // Update zero-bias duration
    smu->setZeroDuration(zeroDuration);
    qDebug() << "K236: Zero duration set to" << zeroDuration << "s";
}


void Measurements::doMeasurement()
{
    qDebug() << "Starting DLTFS measurement sequence...";
    qDebug() << "Parameters: Points=" << totalPoints << ", Integration time=" << tint << "s";
    qDebug() << "Bias=" << biasVoltage << "V, Zero duration=" << zeroDuration << "s";

    if (!analyzer || !analyzer->isConnected()) {
        qWarning() << "HP4291A: Analyzer not connected, cannot start measurement";
        return;
    }

    if (!smu || !smu->isConnected()) {
        qWarning() << "K236: SMU not connected, cannot start measurement";
        return;
    }

    if (!analyzer->write("DISP:TRAC18:CLE\n")) {
        qWarning() << "HP4291A: Failed to clear status for batch";
    }
    QThread::msleep(100);

    // Clear and reset trace buffer
    if (!analyzer->write("*CLS\n")) {
        qWarning() << "HP4291A: Failed to clear status for batch";
    }

    // HP4291A has a 201 point buffer limit
    const int ANALYZER_MAX_POINTS = 201;

    // Calculate number of batches needed
    int numBatches = (totalPoints + ANALYZER_MAX_POINTS - 1) / ANALYZER_MAX_POINTS;
    qDebug() << "HP4291A: Total batches required:" << numBatches;

    // Arrays to store all measurement data
    QList<double> xData;
    QList<double> yData;
    xData.reserve(totalPoints);
    yData.reserve(totalPoints);

    // Pre-pulse baseline: collect data for 1s at DC bias before triggering the pulse
    const int PRE_PULSE_MS = 1000;
    QElapsedTimer prePulseTimer;
    prePulseTimer.start();

    qDebug() << "HP4291A: Collecting pre-pulse baseline for" << PRE_PULSE_MS << "ms...";

    while (prePulseTimer.elapsed() < PRE_PULSE_MS) {
        if (isInterruptionRequested()) return;

        if (!analyzer->write("*CLS;*OPC?\n")) continue;
        analyzer->read();

        if (!analyzer->write("*TRG\n")) continue;

        if (analyzer->write("TRAC:VAL? DTR,1\n")) {
            QString response = analyzer->read(1024);
            if (!response.isEmpty()) {
                int commaPos = response.indexOf(',');
                bool ok;
                double capacitance;
                if (commaPos > 0) {
                    capacitance = response.left(commaPos).toDouble(&ok);
                } else {
                    capacitance = response.trimmed().toDouble(&ok);
                }
                if (ok) {
                    // Negative time = before pulse
                    double t = -(PRE_PULSE_MS - prePulseTimer.elapsed()) / 1000.0;
                    double current = smu->readCurrent();
                    xData.append(t);
                    yData.append(capacitance);
                    emit sendData(t, capacitance, biasVoltage, current);
                }
            }
        }
    }

    qDebug() << "HP4291A: Pre-pulse baseline collected:" << xData.size() << "points";

    // Emit voltage drop to 0V at pulse start
    emit sendData(0.0, yData.isEmpty() ? 0.0 : yData.last(), 0.0, 0.0);

    // Perform zero-bias pulse: drop to 0V, wait, restore bias
    // This creates the transient that the analyzer will capture
    qDebug() << "K236: Triggering zero-bias pulse...";
    if (!smu->trigger()) {
        qWarning() << "K236: Failed to trigger zero-bias pulse";
        return;
    }
    qDebug() << "K236: Zero-bias pulse complete, bias restored";

    // Emit voltage restored to bias at pulse end
    double currentAfterPulse = smu->readCurrent();
    emit sendData(zeroDuration, yData.isEmpty() ? 0.0 : yData.last(), biasVoltage, currentAfterPulse);

    // t=0 is the moment bias is restored (after pulse)
    QElapsedTimer globalTimer;  // For recording time after pulse
    globalTimer.start();

    QElapsedTimer intervalTimer;  // For maintaining TINT intervals
    intervalTimer.start();

    // Process measurements in batches
    for (int batch = 0; batch < numBatches; ++batch) {
        // Calculate batch parameters
        int batchStartPoint = batch * ANALYZER_MAX_POINTS;
        int pointsInBatch = qMin(ANALYZER_MAX_POINTS, totalPoints - batchStartPoint);

        qDebug() << QString("HP4291A: Starting batch %1/%2 (points %3-%4)")
                    .arg(batch + 1)
                    .arg(numBatches)
                    .arg(batchStartPoint + 1)
                    .arg(batchStartPoint + pointsInBatch);

        // Reset analyzer for new batch (except first batch - already configured)
        if (batch > 0) {
            qDebug() << "HP4291A: Resetting analyzer for new batch...";

            // Clear and reset trace buffer
            if (!analyzer->write("*CLS\n")) {
                qWarning() << "HP4291A: Failed to clear status for batch" << batch;
            }

            // Restart interval timer after batch reset delay
            intervalTimer.restart();
        }

        // Collect points for this batch
        for (int i = 0; i < pointsInBatch; ++i) {
            int globalIndex = batchStartPoint + i;
            int batchLocalIndex = i + 1;  // Analyzer point numbering is 1-based

            // Check for thread interruption
            if (isInterruptionRequested()) {
                qWarning() << "HP4291A: Measurement stopped by user at point" << globalIndex;
                return;
            }

            // Clear status and query operation complete
            if (!analyzer->write("*CLS;*OPC?\n")) {
                qWarning() << "HP4291A: Failed to clear status at point" << globalIndex;
                continue;
            }
            analyzer->read();  // Read OPC response

            // Send BUS trigger
            if (!analyzer->write("*TRG\n")) {
                qWarning() << "HP4291A: Failed to send trigger at point" << globalIndex;
                continue;
            }

            // Read measurement value - use batch-local index for TRAC:VAL command
            QString cmd = QString("TRAC:VAL? DTR,%1\n").arg(batchLocalIndex);
            if (analyzer->write(cmd)) {
                QString response = analyzer->read(1024);
                if (!response.isEmpty()) {
                    // Parse Cp value (first value before comma)
                    int commaPos = response.indexOf(',');
                    if (commaPos > 0) {
                        bool ok;
                        double capacitance = response.left(commaPos).toDouble(&ok);
                        if (ok) {
                            yData.append(capacitance);
                        } else {
                            yData.append(0.0);
                            qWarning() << "HP4291A: Failed to parse capacitance at point" << globalIndex;
                        }
                    } else {
                        // No comma, try converting whole response
                        bool ok;
                        double capacitance = response.trimmed().toDouble(&ok);
                        yData.append(ok ? capacitance : 0.0);
                    }
                } else {
                    yData.append(0.0);
                    qWarning() << "HP4291A: Empty response at point" << globalIndex;
                }
            } else {
                yData.append(0.0);
                qWarning() << "HP4291A: Failed to write command at point" << globalIndex;
            }

            // Read current from SMU
            double measuredCurrent = smu->readCurrent();

            // Capture actual elapsed time in seconds (from global start)
            double elapsedTime = globalTimer.elapsed() / 1000.0;
            xData.append(elapsedTime);

            // Emit progress update with data
            emit sendData(xData.last(), yData.last(), biasVoltage, measuredCurrent);

            // Display progress every 10 points or at batch boundaries
            if ((globalIndex + 1) % 10 == 0 || (i + 1) == pointsInBatch) {
                qDebug() << QString("HP4291A: Point %1/%2 (batch %3/%4): %5s, Cp=%6F")
                            .arg(globalIndex + 1)
                            .arg(totalPoints)
                            .arg(batch + 1)
                            .arg(numBatches)
                            .arg(elapsedTime, 0, 'f', 1)
                            .arg(yData.last(), 0, 'E', 3);
            }

            // Maintain timing interval
            qint64 elapsedSinceLastPoint = intervalTimer.elapsed();
            qint64 targetInterval = static_cast<qint64>(tint * 1000);  // TINT in milliseconds
            qint64 remainingTime = targetInterval - elapsedSinceLastPoint;

            if (remainingTime > 0) {
                QThread::msleep(remainingTime);
            }

            // Restart interval timer for next point
            intervalTimer.restart();
        }

        qDebug() << QString("HP4291A: Batch %1/%2 complete").arg(batch + 1).arg(numBatches);
    }

    qDebug() << "Measurement collection complete";
    qDebug() << "Total points collected:" << yData.size();
    qDebug() << "Total time:" << (globalTimer.elapsed() / 1000.0) << "seconds";
}
