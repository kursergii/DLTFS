#include "keithley236smu.h"
#include <QDebug>
#include <QThread>
#include <QRegularExpression>

Keithley236SMU::Keithley236SMU(int board, int address)
    : GpibDevice(board, address)
    , m_biasVoltage(0.0)
    , m_compliance(0.1)
    , m_zeroDuration(0.1)  // Default 100ms zero-bias period
    , m_outputEnabled(false)
{
}

Keithley236SMU::~Keithley236SMU()
{
    if (isConnected()) {
        outputOff();
    }
}

bool Keithley236SMU::initialize()
{
    if (!isConnected()) {
        qWarning() << "K236: Cannot initialize - device not connected";
        return false;
    }

    if (!reset()) {
        qWarning() << "K236: Failed to reset during initialization";
        return false;
    }

    QThread::msleep(500);  // Wait for reset to complete

    // Set to voltage source, DC mode
    if (!sendCommand("F0,0X")) {
        qWarning() << "K236: Failed to set voltage source mode";
        return false;
    }

    qDebug() << "K236: Initialized successfully - voltage source DC mode";
    return true;
}

bool Keithley236SMU::reset()
{
    if (!sendCommand("J0X")) {
        return false;
    }
    qDebug() << "K236: Reset complete";
    return true;
}

bool Keithley236SMU::setVoltageSource(double voltage, double currentCompliance)
{
    // Set source voltage: B<voltage>,0,0X
    QString bCmd = QString("B%1,0,0X").arg(voltage);
    if (!sendCommand(bCmd)) {
        qWarning() << "K236: Failed to set source voltage to" << voltage << "V";
        return false;
    }
    m_biasVoltage = voltage;

    // Set current compliance: L<compliance>,0X
    QString lCmd = QString("L%1,0X").arg(currentCompliance);
    if (!sendCommand(lCmd)) {
        qWarning() << "K236: Failed to set compliance to" << currentCompliance << "A";
        return false;
    }
    m_compliance = currentCompliance;

    qDebug() << "K236: Voltage source set to" << voltage << "V, compliance" << currentCompliance << "A";
    return true;
}

bool Keithley236SMU::setSourceVoltage(double voltage)
{
    QString cmd = QString("B%1,0,0X").arg(voltage);
    if (!sendCommand(cmd)) {
        qWarning() << "K236: Failed to set voltage to" << voltage << "V";
        return false;
    }
    m_biasVoltage = voltage;
    qDebug() << "K236: Voltage set to" << voltage << "V";
    return true;
}

bool Keithley236SMU::outputOn()
{
    if (!sendCommand("N1X")) {
        qWarning() << "K236: Failed to turn output ON";
        return false;
    }
    QThread::msleep(100);  // Allow output to stabilize
    m_outputEnabled = true;
    qDebug() << "K236: Output ON";
    return true;
}

bool Keithley236SMU::outputOff()
{
    if (!sendCommand("N0X")) {
        qWarning() << "K236: Failed to turn output OFF";
        return false;
    }
    m_outputEnabled = false;
    qDebug() << "K236: Output OFF";
    return true;
}

bool Keithley236SMU::trigger()
{
    // Perform zero-bias pulse: drop to 0V, wait, restore bias
    qDebug() << "K236: Triggering zero-bias pulse (duration:" << m_zeroDuration << "s)";

    // Save bias voltage before dropping to zero
    double savedBias = m_biasVoltage;

    // Drop to zero — use sendCommand directly to avoid overwriting m_biasVoltage
    if (!sendCommand("B0.0,0,0X")) {
        qWarning() << "K236: Failed to set zero bias";
        return false;
    }

    // Wait for the zero-bias duration
    int waitMs = static_cast<int>(m_zeroDuration * 1000);
    QThread::msleep(waitMs);

    // Restore bias voltage
    if (!setSourceVoltage(savedBias)) {
        qWarning() << "K236: Failed to restore bias voltage";
        return false;
    }

    qDebug() << "K236: Zero-bias pulse complete, bias restored to" << m_biasVoltage << "V";
    return true;
}

double Keithley236SMU::readCurrent()
{
    if (!isConnected()) {
        return 0.0;
    }

    // Trigger measurement and request reading in one sequence
    // H0X = trigger immediate, G5,2,0X = source+measure values with prefix
    if (!write("H0X")) {
        return 0.0;
    }
    if (!write("G5,2,0X")) {
        return 0.0;
    }

    QString response = read(1024);
    if (response.isEmpty()) {
        return 0.0;
    }

    // Parse response: remove status prefix (e.g. "N" or "NDCV")
    QString trimmed = response.trimmed();
    int numStart = trimmed.indexOf(QRegularExpression("[+-0123456789]"));
    if (numStart > 0) {
        trimmed = trimmed.mid(numStart);
    }

    // Split by comma — first value is voltage, second is current
    QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        bool ok;
        double current = parts[1].toDouble(&ok);
        if (ok) return current;
    } else if (parts.size() == 1) {
        bool ok;
        double current = parts[0].toDouble(&ok);
        if (ok) return current;
    }

    return 0.0;
}

bool Keithley236SMU::sendCommand(const QString& command)
{
    if (!isConnected()) {
        qWarning() << "K236: Cannot send command - device not connected";
        return false;
    }

    qDebug() << "K236: Sending command:" << command;
    bool success = write(command);
    if (!success) {
        qWarning() << "K236: Failed to send command:" << command;
        return false;
    }

    QThread::msleep(10);  // Allow device to process
    return true;
}
