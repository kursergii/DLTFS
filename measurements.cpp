#include "measurements.h"
#include "gpib/gpibdevice.h"
#include "gpib/hp4291analyzer.h"
#include "gpib/hp8114apulser.h"
#include <QSerialPort>
#include <QThread>
#include <cstdio>

Measurements::Measurements(HP8114APulser *pulser, HP4291Analyzer *analyzer, QSerialPort *serialPort,
                           int totalPoints, int maxPointsPerMeas, double tint)
    : m_pulser(pulser)
    , m_analyzer(analyzer)
    , m_serialPort(serialPort)
    , m_totalPoints(totalPoints)
    , m_maxPointsPerMeas(maxPointsPerMeas)
    , m_tint(tint)
    , m_shouldStop(false)
{
}

Measurements::~Measurements()
{
    stopMeasurement();
    wait(); // Wait for thread to finish
}

    // // Create GPIB device instances with specialized classes
    // pulser = new HP8114APulser(0, 14);    // HP8114A Pulser at address 14
    // analyzer = new HP4291Analyzer(0, 17); // HP4291A Analyzer at address 17

    // // Create serial port instance
    // serialPort = new QSerialPort();

    // // Create GPIB polling timer (poll every 100ms)
    // gpibPollTimer = new QTimer(this);
    // gpibPollTimer->setInterval(100);

    // // Connect GPIB polling timer to check for asynchronous data
    // QObject::connect(gpibPollTimer, &QTimer::timeout, this, &MW::onPulserDataReceived);
    // QObject::connect(gpibPollTimer, &QTimer::timeout, this, &MW::onAnalyzerDataReceived);


void Measurements::stopMeasurement()
{
    m_shouldStop = true;
}

bool Measurements::setUpAnalyzerForMeasurement()
{
    // Clear analyzer status
    if (!m_analyzer->write("*CLS\n")) {
        qDebug() << "Failed to clear analyzer status:" << m_analyzer->getLastError();
        return false;
    }

    // Set frequency to 100 MHz with zero span (single frequency)
    if (!m_analyzer->write("SENS:FREQ:SPAN 0;CENT 1.0E8\n")) {
        qDebug() << "Failed to set frequency:" << m_analyzer->getLastError();
        return false;
    }

    // Turn off math functions
    if (!m_analyzer->write("CALC:MATH:STAT OFF\n")) {
        qDebug() << "Failed to turn off math:" << m_analyzer->getLastError();
        return false;
    }

    // Set measurement format to Cp (parallel capacitance)
    if (!m_analyzer->setMeasurementFormat(HP4291Analyzer::CP)) {
        qDebug() << "Failed to set measurement format:" << m_analyzer->getLastError();
        return false;
    }

    // Configure trigger source to GPIB bus (not external)
    if (!m_analyzer->setupTrigger(false)) {
        qDebug() << "Failed to configure trigger:" << m_analyzer->getLastError();
        return false;
    }

    qDebug() << "HP 4291A configured for 100 MHz impedance measurement (Cp format)";
    return true;
}

void Measurements::run()
{
    qDebug() << "Measurements thread: Starting measurement...";

    // Calculate how many measurement batches we need
    int numBatches = (m_totalPoints + m_maxPointsPerMeas - 1) / m_maxPointsPerMeas;

    // Arrays to store all measurement data
    QVector<double> xData(m_totalPoints);   // Time data (actual elapsed time)
    QVector<double> yData(m_totalPoints);   // Capacitance data

    // Send trigger to Arduino (triggers one pulse)
    m_serialPort->write("TRIG 100\n");

    // Start timing - this is the reference point (t=0)
    QElapsedTimer timer;
    timer.start();

    // Pre-allocate buffer for command strings to avoid repeated allocations
    char cmdBuffer[32];

    // Loop through all measurement batches
    for (int batch = 0; batch < numBatches; ++batch) {
        if (m_shouldStop) {
            emit measurementError("Measurement stopped by user");
            return;
        }

        // Calculate how many points to collect in this batch
        int startIndex = batch * m_maxPointsPerMeas;
        int remainingPoints = m_totalPoints - startIndex;
        int batchSize = (remainingPoints < m_maxPointsPerMeas) ? remainingPoints : m_maxPointsPerMeas;

        qDebug() << QString("Measurements thread: Starting batch %1/%2 (%3 points)")
                    .arg(batch + 1)
                    .arg(numBatches)
                    .arg(batchSize);

        // If not the first batch, reconfigure analyzer
        if (batch > 0) {
            if (!setUpAnalyzerForMeasurement()) {
                emit measurementError("Failed to reconfigure analyzer for batch " + QString::number(batch + 1));
                return;
            }
        }

        // Collect measurements for this batch
        for (int i = 0; i < batchSize; ++i) {
            if (m_shouldStop) {
                emit measurementError("Measurement stopped by user");
                return;
            }

            int globalIndex = startIndex + i;  // Index in the full data arrays

            // Combine commands to reduce GPIB transactions
            // Send: clear, trigger, and wait for completion in one sequence
            m_analyzer->write("*CLS;*TRG;*OPC?\n");
            m_analyzer->read();  // Read OPC response (blocks until measurement complete)

            // Read measurement value (returns Cp,D)
            // Use snprintf for faster string formatting
            // Note: analyzer's internal point numbering starts at 1
            snprintf(cmdBuffer, sizeof(cmdBuffer), "TRAC:VAL? DTR,%d\n", i + 1);

            if (m_analyzer->write(cmdBuffer)) {
                QString response = m_analyzer->read(1024);
                if (!response.isEmpty()) {
                    // Fast parse: find first comma, convert substring before it
                    int commaPos = response.indexOf(',');
                    if (commaPos > 0) {
                        bool ok;
                        yData[globalIndex] = response.left(commaPos).toDouble(&ok);
                        if (!ok) {
                            yData[globalIndex] = 0.0;
                        }
                    } else {
                        // No comma, try converting whole string
                        bool ok;
                        yData[globalIndex] = response.trimmed().toDouble(&ok);
                        if (!ok) {
                            yData[globalIndex] = 0.0;
                        }
                    }
                } else {
                    yData[globalIndex] = 0.0;
                }
            } else {
                yData[globalIndex] = 0.0;
            }

            // Capture actual elapsed time in seconds
            double elapsedTime = timer.elapsed() / 1000.0;  // Convert ms to seconds
            xData[globalIndex] = elapsedTime;

            // Emit progress update
            emit progressUpdate(globalIndex + 1, m_totalPoints, elapsedTime, yData[globalIndex]);

            // Maintain timing interval (wait until TINT seconds have passed)
            qint64 targetTime = static_cast<qint64>(globalIndex * m_tint * 1000);  // Target time in ms
            qint64 currentTime = timer.elapsed();
            qint64 remainingTime = targetTime + static_cast<qint64>(m_tint * 1000) - currentTime;

            if (remainingTime > 0) {
                QThread::msleep(remainingTime);
            }
        }
    }

    qDebug() << "Measurements thread: Measurement complete";
    emit measurementComplete(xData, yData);
}


void MW::connectPulser() {

    // Connect to pulser (GPIB address 14)
    if (pulser->connect()) {
        QString pulserID = pulser->queryIdentification();
        qDebug() << "Pulser ID:" << pulserID;
    } else {
        qDebug() << "Failed to connect to pulser:" << pulser->getLastError();
    }

    if (pulser->setExternalTrigger()) {
        qDebug() << "HP8114A configured for external trigger from Arduino";
    } else {
        qDebug() << "Failed to configure HP8114A for external trigger:" << pulser->getLastError();
    }

    // Set pulse period to 0.5 second
    if (pulser->write(":PULS:PER 0.5\n")) {
        qDebug() << "Pulse period set to 0.5 second";
    } else {
        qDebug() << "Failed to set pulse period:" << pulser->getLastError();
    }

    // Use default values (TODO: add UI inputs back)
    double ampl = 5.0;  // 5V amplitude
    double duri = 100e-6;  // 100 microseconds

    QString inst = ":VOLT " + QString::number(ampl) + "V\nPULSe:WIDTh " + QString::number(duri) + "\nOUTPut ON\n";

    if (pulser->write(inst)){
        qDebug() << "HP8114A configured for pulse";
    }
    else {
        qDebug() << "HP8114A not configured for pulse";
    }

}

void MW::connectAnalyzer() {

    // Connect to HP 4291A RF Impedance Analyzer (GPIB address 17)
    // Frequency range: 1 MHz to 1.8 GHz
    if (analyzer->connect()) {
        QString analyzerID = analyzer->queryIdentification();
        qDebug() << "Analyzer ID:" << analyzerID;
    } else {
        qDebug() << "Failed to connect to analyzer:" << analyzer->getLastError();
    }

}

void MW::connectArduino() {

    // Connect Arduino for TRIGering

    if (serialPort->isOpen()) {
        serialPort->close();
        qDebug() << "Arduino disconnected";
        return;
    }

    QString portName = ui->arduinoPortCombo->currentText();
    if (portName.isEmpty()) {
        qDebug() << "Error: No Arduino port selected!";
        return;
    }

    serialPort->setPortName(portName);
    serialPort->setBaudRate(QSerialPort::Baud9600);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "Arduino connected on" << portName;
    } else {
        qDebug() << "Failed to connect to Arduino:" << serialPort->errorString();
    }


}

bool MW::setUpAnalyzerForMeasurement() {
    // Configure HP 4291A for impedance measurement at 100 MHz
    // Using specialized HP4291Analyzer methods
    qDebug() << "Configuring HP 4291A for impedance measurement at 100 MHz...";

    // Setup measurement at 100 MHz
    if (!analyzer->setupForMeasurement(100e6)) {
        qDebug() << "Failed to setup measurement:" << analyzer->getLastError();
        return false;
    }

    // Set measurement format to Cp-D (capacitance parallel and dissipation factor)
    if (!analyzer->setMeasurementFormat(HP4291Analyzer::CP)) {
        qDebug() << "Failed to set measurement format:" << analyzer->getLastError();
        return false;
    }

    // Configure trigger source to GPIB bus (not external)
    if (!analyzer->setupTrigger(false)) {
        qDebug() << "Failed to configure trigger:" << analyzer->getLastError();
        return false;
    }

    qDebug() << "HP 4291A configured for 100 MHz impedance measurement (Cp format)";
    return true;
}

void MW::onMeasurementProgress(int currentPoint, int totalPoints, double time, double capacitance)
{
    // Display progress (runs in main thread, so UI updates are safe)
    qDebug() << QString("Point %1/%2: %3 [SEC] - Cp: %4 pF")
                .arg(currentPoint, 3)
                .arg(totalPoints)
                .arg(time, 7, 'f', 2)
                .arg(capacitance * 1e12, 0, 'E', 6);
}

void MW::onMeasurementComplete(QVector<double> xData, QVector<double> yData)
{
    qDebug() << "\n=== Measurement complete (received in main thread) ===";
    qDebug() << "Time data (first 5 points):" << xData[0] << xData[1] << xData[2] << xData[3] << xData[4];
    qDebug() << "Capacitance data (first 5 points, pF):"
             << yData[0]*1e12 << yData[1]*1e12 << yData[2]*1e12 << yData[3]*1e12 << yData[4]*1e12;

    // Display last 5 points (works for any size)
    int lastIdx = xData.size() - 1;
    if (lastIdx >= 4) {
        qDebug() << "Time data (last 5 points):"
                 << xData[lastIdx-4] << xData[lastIdx-3] << xData[lastIdx-2] << xData[lastIdx-1] << xData[lastIdx];
        qDebug() << "Capacitance data (last 5 points, pF):"
                 << yData[lastIdx-4]*1e12 << yData[lastIdx-3]*1e12 << yData[lastIdx-2]*1e12
                 << yData[lastIdx-1]*1e12 << yData[lastIdx]*1e12;
    }

    // Update the plot with collected data (safe to call from main thread)
    updatePlot(xData, yData);
}

void MW::onMeasurementError(QString error)
{
    qDebug() << "Measurement error:" << error;
}

void MW::onSerialDataReceived()
{
    // Read available data from serial port
    QByteArray data = serialPort->readAll();
    serialBuffer.append(QString::fromUtf8(data));

    // Process complete lines (ending with \n or \r\n)
    while (serialBuffer.contains('\n')) {
        int newlinePos = serialBuffer.indexOf('\n');
        QString line = serialBuffer.left(newlinePos).trimmed();
        serialBuffer.remove(0, newlinePos + 1);

        if (!line.isEmpty()) {
            qDebug() << "Arduino message received:" << line;

            // Check for trigger command (software trigger mode)
            // Note: When HP8114A is in external trigger mode, it responds to
            // hardware trigger pulses directly. This software trigger is a fallback
            // or can be used for testing without hardware connections.
            if (line == "TRIG 1") {
                qDebug() << "Software trigger command received! Initiating pulse...";
                onStartButtonClicked();
            }
        }
    }
}