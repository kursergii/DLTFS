#include "measurements.h"
#include <QTimer>
#include <QEventLoop>
#include <QElapsedTimer>

void Measurements::running() {
    // Placeholder for the main loop logic 
    waiter(1000);
    qDebug() << "Measurements running...";
    if (isMeasuring) {
        doMeasurement();
        emit isDone(true);
        isMeasuring = false;
    }
    else if (isReadyForInitialMeasurement) {
        applyPulseSettings();
        isReadyForInitialMeasurement = false;
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

bool Measurements::connectPulser() {
    // Connect to pulser (GPIB address 14)
    if (!pulser->connect()) {
        qDebug() << "Failed to connect to pulser:" << pulser->getLastError();
        return false;
    }   
    
    QString pulserID = pulser->getIdentification();
    qDebug() << "Pulser ID:" << pulserID;
    qDebug() << "Pulser initialized successfully";

    // Initialize the pulser (includes reset and configuration)
    if (!pulser->initialize()) {
        qDebug() << "Failed to initialize pulser";
        return false;
    }

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

    // Step 3: Setup measurement frequency (10 MHz for DLTFS)
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

bool Measurements::connectArduino() {
    // Connect Arduino for triggering
    serialPort->setPortName("/dev/ttyUSB0");  // Default port, should be configurable
    serialPort->setBaudRate(QSerialPort::Baud9600);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "Failed to connect to Arduino:" << serialPort->errorString();
        return false;
    }

    qDebug() << "Arduino connected";
    return true;
}

void Measurements::updatePulseParams(double offset, double amplitude, double duration) {
    pulseOffset = offset = 0;
    pulseAmplitude = amplitude;
    pulseDuration = duration;
    qDebug() << "Pulse params updated: Offset=" << offset << "V, Amplitude=" << amplitude << "V, Duration=" << duration << "us";

    // If devices are connected, apply the new settings
    if (pulser && pulser->isConnected()) {
        applyPulseSettings();
    }
}

void Measurements::updateMeasurementParams(int numPoints, double integrationTime) {
    totalPoints = numPoints;
    tint = integrationTime;
    qDebug() << "Measurement params updated: Points=" << numPoints << ", Integration time=" << integrationTime << "s";
}

void Measurements::updateAnalyzerFrequency(double frequencyMHz) {
    if (!analyzer || !analyzer->isConnected()) {
        qDebug() << "Cannot update analyzer frequency: Analyzer not connected";
        return;
    }

    // Convert MHz to Hz
    double frequencyHz = frequencyMHz * 1e6;

    // Update analyzer frequency
    if (analyzer->setupForMeasurement(frequencyHz)) {
        qDebug() << "Analyzer frequency updated to" << frequencyMHz << "MHz";
    } else {
        qWarning() << "Failed to update analyzer frequency to" << frequencyMHz << "MHz";
    }
}

void Measurements::applyPulseSettings() {
    if (!pulser || !pulser->isConnected()) {
        qDebug() << "Cannot apply pulse settings: Pulser not connected";
        return;
    }

    // Calculate high and low voltage levels
    double highLevel = pulseOffset + pulseAmplitude;
    double lowLevel = pulseOffset;
    double widthSeconds = pulseDuration * 1e-6;  // Convert microseconds to seconds

    // Use the proper HP8114APulser API methods
    if (pulser->setVoltageAmplitude(pulseAmplitude)) {
        qDebug() << "Set pulse Amplitude to" << pulseAmplitude << "V";
    } else {
        qDebug() << "Failed to set pulse high level";
    }

    // if (pulser->setVoltageLow(lowLevel)) {
    //     qDebug() << "Set pulse low level (offset) to" << lowLevel << "V";
    // } else {
    //     qDebug() << "Failed to set pulse low level";
    // }

    if (pulser->setPulseWidth(widthSeconds)) {
        qDebug() << "Set pulse width to" << pulseDuration << "us";
    } else {
        qDebug() << "Failed to set pulse width";
    }
}

// bool Measurements::setUpAnalyzerForMeasurement()
// {
//     // Clear analyzer status
//     if (!m_analyzer->write("*CLS\n")) {
//         qDebug() << "Failed to clear analyzer status:" << m_analyzer->getLastError();
//         return false;
//     }

//     // Set frequency to 100 MHz with zero span (single frequency)
//     if (!m_analyzer->write("SENS:FREQ:SPAN 0;CENT 1.0E8\n")) {
//         qDebug() << "Failed to set frequency:" << m_analyzer->getLastError();
//         return false;
//     }

//     // Turn off math functions
//     if (!m_analyzer->write("CALC:MATH:STAT OFF\n")) {
//         qDebug() << "Failed to turn off math:" << m_analyzer->getLastError();
//         return false;
//     }

//     // Set measurement format to Cp (parallel capacitance)
//     if (!m_analyzer->setMeasurementFormat(HP4291Analyzer::CP)) {
//         qDebug() << "Failed to set measurement format:" << m_analyzer->getLastError();
//         return false;
//     }

//     // Configure trigger source to GPIB bus (not external)
//     if (!m_analyzer->setupTrigger(false)) {
//         qDebug() << "Failed to configure trigger:" << m_analyzer->getLastError();
//         return false;
//     }

//     qDebug() << "HP 4291A configured for 100 MHz impedance measurement (Cp format)";
//     return true;
// }


void Measurements::doMeasurement()
{
    qDebug() << "HP4291A: Starting DLTFS measurement sequence...";
    qDebug() << "Parameters: Points=" << totalPoints << ", Integration time=" << tint << "s";

    if (!analyzer || !analyzer->isConnected()) {
        qWarning() << "HP4291A: Analyzer not connected, cannot start measurement";
        return;
    }

    if (!pulser || !pulser->isConnected()) {
        qWarning() << "HP8114A: Pulser not connected, cannot start measurement";
        return;
    }
    
    if (!analyzer->write("DISP:TRAC18:CLE\n")) {
        qWarning() << "HP4291A: Failed to clear status for batch";
    }
    // Small delay to allow Arduino to process trigger
    QThread::msleep(100);

        // Clear and reset trace buffer
    if (!analyzer->write("*CLS\n")) {
        qWarning() << "HP4291A: Failed to clear status for batch";
    }

    // // Small delay to ensure pulse completes
    // QThread::msleep(50);

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

    // Start timing - this is the reference point (t=0)
    QElapsedTimer globalTimer;  // For recording absolute time
    globalTimer.start();

    QElapsedTimer intervalTimer;  // For maintaining TINT intervals (matches raw code)
    intervalTimer.start();

        // // Send one initial pulse from pulser before starting measurements
    qDebug() << "HP8114A: Triggering initial pulse...";
    if (!pulser->trigger()) {
        qWarning() << "HP8114A: Failed to trigger initial pulse";
        return;
    }
    qDebug() << "HP8114A: Initial pulse triggered successfully";
    
    bool pulseTrigger = true;
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

            // Small delay to ensure analyzer is ready
            // QThread::msleep(100);

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

            // Clear status and query operation complete (matches HP BASIC line 430)
            if (!analyzer->write("*CLS;*OPC?\n")) {
                qWarning() << "HP4291A: Failed to clear status at point" << globalIndex;
                continue;
            }
            analyzer->read();  // Read OPC response

            // Send BUS trigger (matches HP BASIC line 460: TRIGGER @Hp4291)
            if (!analyzer->write("*TRG\n")) {
                qWarning() << "HP4291A: Failed to send trigger at point" << globalIndex;
                continue;
            }

            // Trigger pulse from pulser
            if (pulseTrigger) {
                if (!pulser->trigger()) {
                    qWarning() << "HP8114A: Failed to trigger pulse at point" << globalIndex;
                }
                    // Send trigger command to Arduino
            if (serialPort && serialPort->isOpen()) {
                qDebug() << "Arduino: Sending trigger command 'TRIG 100'";
                qint64 bytesWritten = serialPort->write("TRIG 100\n");
                if (bytesWritten == -1) {
                    qWarning() << "Arduino: Failed to send trigger command";
                } else {
                    serialPort->flush();  // Ensure data is sent immediately
                    qDebug() << "Arduino: Trigger command sent successfully";
                }
            } else {
                qWarning() << "Arduino: Serial port not available";
            }
                pulseTrigger = false;
            }

            // Small delay to allow trigger to complete
            // QThread::msleep(100);

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

            // Capture actual elapsed time in seconds (from global start)
            double elapsedTime = globalTimer.elapsed() / 1000.0;
            xData.append(elapsedTime);

            // Emit progress update with data
            emit sendData(xData.last(), yData.last());

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

            // Maintain timing interval (matches raw code logic - wait for TINT since last point)
            qint64 elapsedSinceLastPoint = intervalTimer.elapsed();
            qint64 targetInterval = static_cast<qint64>(tint * 1000);  // TINT in milliseconds
            qint64 remainingTime = targetInterval - elapsedSinceLastPoint;

            if (remainingTime > 0) {
                QThread::msleep(remainingTime);
            }

            // Restart interval timer for next point (matches raw code t1 = t2)
            intervalTimer.restart();
        }

        qDebug() << QString("HP4291A: Batch %1/%2 complete").arg(batch + 1).arg(numBatches);
    }

    qDebug() << "HP4291A: Measurement collection complete";
    qDebug() << "HP4291A: Total points collected:" << yData.size();
    qDebug() << "HP4291A: Total time:" << (globalTimer.elapsed() / 1000.0) << "seconds";
}
