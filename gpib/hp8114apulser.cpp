#include "hp8114apulser.h"
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>

HP8114APulser::HP8114APulser(int board, int address)
    : GpibDevice(board, address)
    , m_voltageHigh(0.0)
    , m_voltageLow(0.0)
    , m_voltageAmplitude(0.0)
    , m_voltageBaseline(0.0)
    , m_pulseWidth(0.0)
    , m_pulsePeriod(0.0)
    , m_pulseFrequency(0.0)
    , m_pulseDutyCycle(0.0)
    , m_triggerSource(IMMEDIATE)
    , m_triggerSlope(POSITIVE)
    , m_outputEnabled(false)
{
}

HP8114APulser::~HP8114APulser()
{
    // Safely disable output on destruction
    if (isConnected()) {
        disableOutput();
    }
}

// ============================================================================
// Initialization and Reset
// ============================================================================

bool HP8114APulser::initialize()
{
    if (!isConnected()) {
        qWarning() << "HP8114A: Cannot initialize - device not connected";
        return false;
    }

    // Clear status
    if (!clearStatus()) {
        qWarning() << "HP8114A: Failed to clear status during initialization";
        return false;
    }

    // Reset to known state
    if (!reset()) {
        qWarning() << "HP8114A: Failed to reset during initialization";
        return false;
    }
    
    if (!enableOutput(false)) {
        qWarning() << "HP8114A: Failed to disable output during initialization";
        return false;
    }

    if (!setTriggerSource(EXTERNAL)) {
        qWarning() << "HP8114A: Failed to set trigger source during initialization";
        return false;
    }
    if (!setTriggerSlope(POSITIVE)) {
        qWarning() << "HP8114A: Failed to set trigger slope during initialization";
        return false;
    }
    if (!setOutputPolarity(POLARITY_POSITIVE)) {
        qWarning() << "HP8114A: Failed to set output polarity during initialization";
        return false;
    }
    if (!setPulsePeriod(0.99)) {  // Default 10ms period
        qWarning() << "HP8114A: Failed to set default pulse period during initialization";
        return false;
    }
    if (!setPulseWidth(0.001)) { // Default 100us width
        qWarning() << "HP8114A: Failed to set default pulse width during initialization";
        return false;
    }
    if (!setVoltageAmplitude(1.0)) { // Default 1V amplitude
        qWarning() << "HP8114A: Failed to set default voltage amplitude during initialization";
        return false;
    }
    if (!enableOutput(true)) {
        qWarning() << "HP8114A: Failed to ensure output enabled during initialization";
        return false;
    }

    // Give device time to process the mode change
    QThread::msleep(100);

    qDebug() << "HP8114A: Initialized successfully - ready for voltage configuration";
    return true;
}

bool HP8114APulser::reset()
{
    if (!sendCommand("*RST")) {
        return false;
    }

    // Wait for reset to complete (HP 8114A needs time to reset)
    // Using simple delay instead of *OPC? which may not work after reset
    QThread::msleep(500);  // 500ms should be sufficient for reset

    qDebug() << "HP8114A: Reset complete";
    return true;
}

bool HP8114APulser::clearStatus()
{
    return sendCommand("*CLS");
}

QString HP8114APulser::getIdentification()
{
    QString id = queryCommand("*IDN?");
    qDebug() << "HP8114A Identification:" << id;
    return id;
}

// ============================================================================
// Voltage Configuration
// ============================================================================

bool HP8114APulser::setVoltageHigh(double voltage)
{
    if (!validateVoltage(voltage)) {
        qWarning() << "HP8114A: Invalid voltage value:" << voltage;
        return false;
    }

    // HP 8114A requires unit suffix (V for volts)
    QString cmd = QString(":VOLT:HIGH %1V").arg(voltage);
    if (sendCommand(cmd)) {
        m_voltageHigh = voltage;
        qDebug() << "HP8114A: Voltage HIGH set to" << voltage << "V";
        return true;
    }
    return false;
}

bool HP8114APulser::setVoltageLow(double voltage)
{
    if (!validateVoltage(voltage)) {
        qWarning() << "HP8114A: Invalid voltage value:" << voltage;
        return false;
    }

    // HP 8114A requires unit suffix (V for volts)
    QString cmd = QString(":VOLT:LOW %1V").arg(voltage);
    if (sendCommand(cmd)) {
        m_voltageLow = voltage;
        qDebug() << "HP8114A: Voltage LOW set to" << voltage << "V";
        return true;
    }
    return false;
}

bool HP8114APulser::setVoltageAmplitude(double amplitude)
{
    if (!validateVoltage(amplitude)) {
        qWarning() << "HP8114A: Invalid amplitude value:" << amplitude;
        return false;
    }

    // HP 8114A uses :VOLT for amplitude
    QString cmd = QString(":VOLT %1V").arg(amplitude);
    if (sendCommand(cmd)) {
        m_voltageAmplitude = amplitude;
        qDebug() << "HP8114A: Voltage amplitude set to" << amplitude << "V";
        return true;
    }
    return false;
}

bool HP8114APulser::setVoltageBaseline(double baseline)
{
    if (!validateVoltage(baseline)) {
        qWarning() << "HP8114A: Invalid baseline voltage:" << baseline;
        return false;
    }

    QString cmd = QString(":VOLT:BAS %1V").arg(baseline);
    if (sendCommand(cmd)) {
        m_voltageBaseline = baseline;
        qDebug() << "HP8114A: Voltage baseline set to" << baseline << "V";
        return true;
    }
    return false;
}

bool HP8114APulser::setVoltageLimits(double high_limit, double low_limit)
{
    QString cmdHigh = QString(":VOLT:LIM:HIGH %1V").arg(high_limit);
    QString cmdLow = QString(":VOLT:LIM:LOW %1V").arg(low_limit);

    if (sendCommand(cmdHigh) && sendCommand(cmdLow)) {
        qDebug() << "HP8114A: Voltage limits set to [" << low_limit << "," << high_limit << "] V";
        return true;
    }
    return false;
}

bool HP8114APulser::enableVoltageLimits(bool enable)
{
    QString cmd = QString(":VOLT:LIM:STAT %1").arg(enable ? "ON" : "OFF");
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Voltage limits" << (enable ? "enabled" : "disabled");
        return true;
    }
    return false;
}

// ============================================================================
// Pulse Timing Configuration
// ============================================================================

bool HP8114APulser::setPulseWidth(double width_sec)
{
    if (!validateTiming(width_sec)) {
        qWarning() << "HP8114A: Invalid pulse width:" << width_sec;
        return false;
    }

    // Convert to nanoseconds and add unit suffix
    double width_ns = width_sec * 1e9;
    QString cmd = QString(":PULS:WIDT %1NS").arg(width_ns);
    if (sendCommand(cmd)) {
        m_pulseWidth = width_sec;
        qDebug() << "HP8114A: Pulse width set to" << width_ns << "ns";
        return true;
    }
    return false;
}

bool HP8114APulser::setPulsePeriod(double period_sec)
{
    if (!validateTiming(period_sec)) {
        qWarning() << "HP8114A: Invalid pulse period:" << period_sec;
        return false;
    }

    // Convert to milliseconds and add unit suffix
    double period_ms = period_sec * 1e3;
    QString cmd = QString(":PULS:PER %1MS").arg(period_ms);
    if (sendCommand(cmd)) {
        m_pulsePeriod = period_sec;
        m_pulseFrequency = 1.0 / period_sec;
        qDebug() << "HP8114A: Pulse period set to" << period_ms << "ms ("
                 << m_pulseFrequency << "Hz)";
        return true;
    }
    return false;
}

bool HP8114APulser::setPulseFrequency(double freq_hz)
{
    if (freq_hz <= 0.0) {
        qWarning() << "HP8114A: Invalid frequency:" << freq_hz;
        return false;
    }

    QString cmd = QString(":FREQ %1HZ").arg(freq_hz);
    if (sendCommand(cmd)) {
        m_pulseFrequency = freq_hz;
        m_pulsePeriod = 1.0 / freq_hz;
        qDebug() << "HP8114A: Pulse frequency set to" << freq_hz << "Hz";
        return true;
    }
    return false;
}

bool HP8114APulser::setPulseDutyCycle(double duty_percent)
{
    if (duty_percent < 0.0 || duty_percent > 100.0) {
        qWarning() << "HP8114A: Invalid duty cycle:" << duty_percent;
        return false;
    }

    QString cmd = QString(":PULS:DCYC %1PCT").arg(duty_percent);
    if (sendCommand(cmd)) {
        m_pulseDutyCycle = duty_percent;
        qDebug() << "HP8114A: Pulse duty cycle set to" << duty_percent << "%";
        return true;
    }
    return false;
}

bool HP8114APulser::setPulseDelay(double delay_sec)
{
    if (!validateTiming(delay_sec)) {
        qWarning() << "HP8114A: Invalid pulse delay:" << delay_sec;
        return false;
    }

    double delay_ns = delay_sec * 1e9;
    QString cmd = QString(":PULS:DEL %1NS").arg(delay_ns);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Pulse delay set to" << delay_ns << "ns";
        return true;
    }
    return false;
}

bool HP8114APulser::setTrailingEdgeDelay(double delay_sec)
{
    if (!validateTiming(delay_sec)) {
        qWarning() << "HP8114A: Invalid trailing edge delay:" << delay_sec;
        return false;
    }

    double delay_ns = delay_sec * 1e9;
    QString cmd = QString(":PULS:TDEL %1NS").arg(delay_ns);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Trailing edge delay set to" << delay_ns << "ns";
        return true;
    }
    return false;
}

bool HP8114APulser::setPulseParameters(double high_v, double low_v, double width_sec, double period_sec)
{
    qDebug() << "HP8114A: Setting pulse parameters:";
    qDebug() << "  HIGH:" << high_v << "V, LOW:" << low_v << "V";
    qDebug() << "  Width:" << (width_sec * 1e6) << "us, Period:" << (period_sec * 1e3) << "ms";

    bool success = true;
    success &= setVoltageHigh(high_v);
    success &= setVoltageLow(low_v);
    success &= setPulseWidth(width_sec);
    success &= setPulsePeriod(period_sec);

    if (success) {
        qDebug() << "HP8114A: All pulse parameters set successfully";
    } else {
        qWarning() << "HP8114A: Some pulse parameters failed to set";
    }

    return success;
}

// ============================================================================
// Trigger Configuration
// ============================================================================

bool HP8114APulser::setTriggerSource(TriggerSource source)
{
    QString sourceStr;
    switch (source) {
        case IMMEDIATE:
            sourceStr = "IMMediate";
            break;
        case EXTERNAL:
            sourceStr = "EXTernal";
            break;
        case MANUAL:
            sourceStr = "MANual";
            break;
        default:
            qWarning() << "HP8114A: Invalid trigger source";
            return false;
    }

    QString cmd = QString(":TRIG:SOUR %1").arg(sourceStr);
    if (sendCommand(cmd)) {
        m_triggerSource = source;
        qDebug() << "HP8114A: Trigger source set to" << sourceStr;
        return true;
    }
    return false;
}

bool HP8114APulser::setTriggerSlope(TriggerSlope slope)
{
    QString slopeStr;
    switch (slope) {
        case POSITIVE:
            slopeStr = "POS";
            break;
        case NEGATIVE:
            slopeStr = "NEG";
            break;
        case EITHER:
            slopeStr = "EITH";
            break;
        default:
            qWarning() << "HP8114A: Invalid trigger slope";
            return false;
    }

    QString cmd = QString(":TRIG:SLOP %1").arg(slopeStr);
    if (sendCommand(cmd)) {
        m_triggerSlope = slope;
        qDebug() << "HP8114A: Trigger slope set to" << slopeStr;
        return true;
    }
    return false;
}

bool HP8114APulser::setTriggerLevel(double level_v)
{
    QString cmd = QString(":TRIG:LEV %1V").arg(level_v);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Trigger level set to" << level_v << "V";
        return true;
    }
    return false;
}

bool HP8114APulser::setTriggerCount(int count)
{
    if (count < 1) {
        qWarning() << "HP8114A: Invalid trigger count:" << count;
        return false;
    }

    QString cmd = QString(":TRIG:COUN %1").arg(count);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Trigger count set to" << count;
        return true;
    }
    return false;
}

bool HP8114APulser::trigger()
{
    if (sendCommand("*TRG")) {
        qDebug() << "HP8114A: Manual trigger sent";
        return true;
    }
    return false;
}

bool HP8114APulser::enableTriggerInhibit(bool enable)
{
    QString cmd = QString(":TRIG:INH %1").arg(enable ? "ON" : "OFF");
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Trigger inhibit" << (enable ? "enabled" : "disabled");
        return true;
    }
    return false;
}

bool HP8114APulser::setTriggerInhibitMode(TriggerInhibitMode mode)
{
    QString modeStr;
    switch (mode) {
        case INHIBIT_RISE:
            modeStr = "RIS";
            break;
        case INHIBIT_FALL:
            modeStr = "FALL";
            break;
        case INHIBIT_HIGH:
            modeStr = "HIGH";
            break;
        case INHIBIT_LOW:
            modeStr = "LOW";
            break;
        default:
            qWarning() << "HP8114A: Invalid trigger inhibit mode";
            return false;
    }

    QString cmd = QString(":TRIG:INH:MODE %1").arg(modeStr);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Trigger inhibit mode set to" << modeStr;
        return true;
    }
    return false;
}

bool HP8114APulser::enableExternalWidth(bool enable)
{
    QString cmd = QString(":TRIG:EWID %1").arg(enable ? "ON" : "OFF");
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: External width mode" << (enable ? "enabled" : "disabled");
        return true;
    }
    return false;
}

// ============================================================================
// Output Control
// ============================================================================

bool HP8114APulser::enableOutput(bool enable)
{
    QString cmd = QString(":OUTP %1").arg(enable ? "ON" : "OFF");
    if (sendCommand(cmd)) {
        m_outputEnabled = enable;
        qDebug() << "HP8114A: Output" << (enable ? "enabled" : "disabled");
        return true;
    }
    return false;
}

bool HP8114APulser::disableOutput()
{
    return enableOutput(false);
}

bool HP8114APulser::isOutputEnabled()
{
    QString response = queryCommand(":OUTP?");
    if (response.isEmpty()) {
        return m_outputEnabled; // Return cached value on error
    }

    m_outputEnabled = (response.trimmed() == "1" || response.trimmed().toUpper() == "ON");
    return m_outputEnabled;
}

bool HP8114APulser::setInternalImpedance(double ohms)
{
    QString cmd = QString(":OUTP:IMP:INT %1OHM").arg(ohms);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Internal impedance set to" << ohms << "Ω";
        return true;
    }
    return false;
}

bool HP8114APulser::setExternalImpedance(double ohms)
{
    QString cmd = QString(":OUTP:IMP:EXT %1OHM").arg(ohms);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: External impedance set to" << ohms << "Ω";
        return true;
    }
    return false;
}

bool HP8114APulser::setOutputPolarity(OutputPolarity polarity)
{
    QString polarityStr = (polarity == POLARITY_POSITIVE) ? "POS" : "NEG";
    QString cmd = QString(":OUTP:POL %1").arg(polarityStr);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Output polarity set to" << polarityStr;
        return true;
    }
    return false;
}

// ============================================================================
// Query Methods
// ============================================================================

double HP8114APulser::queryVoltageHigh()
{
    double value = queryDouble(":VOLT:HIGH?");
    if (value != 0.0) {
        m_voltageHigh = value;
    }
    return value;
}

double HP8114APulser::queryVoltageLow()
{
    double value = queryDouble(":VOLT:LOW?");
    if (value != 0.0) {
        m_voltageLow = value;
    }
    return value;
}

double HP8114APulser::queryVoltageAmplitude()
{
    double value = queryDouble(":VOLT?");
    if (value != 0.0) {
        m_voltageAmplitude = value;
    }
    return value;
}

double HP8114APulser::queryVoltageBaseline()
{
    double value = queryDouble(":VOLT:BAS?");
    m_voltageBaseline = value;
    return value;
}

double HP8114APulser::queryPulseWidth()
{
    double value = queryDouble(":PULS:WIDT?");
    if (value > 0.0) {
        m_pulseWidth = value;
    }
    return value;
}

double HP8114APulser::queryPulsePeriod()
{
    double value = queryDouble(":PULS:PER?");
    if (value > 0.0) {
        m_pulsePeriod = value;
        m_pulseFrequency = 1.0 / value;
    }
    return value;
}

double HP8114APulser::queryPulseFrequency()
{
    double value = queryDouble(":FREQ?");
    if (value > 0.0) {
        m_pulseFrequency = value;
        m_pulsePeriod = 1.0 / value;
    }
    return value;
}

QString HP8114APulser::queryTriggerSource()
{
    return queryCommand(":TRIG:SOUR?");
}

QString HP8114APulser::queryTriggerSlope()
{
    return queryCommand(":TRIG:SLOP?");
}

// ============================================================================
// Status and Error Handling
// ============================================================================

bool HP8114APulser::checkOperationComplete()
{
    return queryBool("*OPC?");
}

bool HP8114APulser::waitForOperationComplete(int timeout_ms)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms) {
        if (checkOperationComplete()) {
            return true;
        }
        QThread::msleep(10); // Wait 10ms between checks
    }

    qWarning() << "HP8114A: Timeout waiting for operation complete";
    return false;
}

QString HP8114APulser::getSystemError()
{
    QString error = queryCommand(":SYST:ERR?");
    if (!error.isEmpty() && !error.startsWith("0,")) {
        qWarning() << "HP8114A System Error:" << error;
    }
    return error;
}

bool HP8114APulser::hasError()
{
    QString error = getSystemError();
    return !error.isEmpty() && !error.startsWith("0,");
}

int HP8114APulser::readEventStatusRegister()
{
    return queryInt("*ESR?");
}

bool HP8114APulser::setEventStatusEnable(int mask)
{
    QString cmd = QString("*ESE %1").arg(mask);
    return sendCommand(cmd);
}

int HP8114APulser::readStatusByte()
{
    return queryInt("*STB?");
}

// ============================================================================
// Memory Operations
// ============================================================================

bool HP8114APulser::saveSettings(int location)
{
    if (location < 1 || location > 9) {
        qWarning() << "HP8114A: Invalid save location (must be 1-9):" << location;
        return false;
    }

    QString cmd = QString("*SAV %1").arg(location);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Settings saved to location" << location;
        return waitForOperationComplete();
    }
    return false;
}

bool HP8114APulser::recallSettings(int location)
{
    if (location < 0 || location > 9) {
        qWarning() << "HP8114A: Invalid recall location (must be 0-9):" << location;
        return false;
    }

    QString cmd = QString("*RCL %1").arg(location);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Settings recalled from location" << location;

        // Update cached values after recall
        if (waitForOperationComplete()) {
            updateCachedValues();
            return true;
        }
    }
    return false;
}

// ============================================================================
// Display Control
// ============================================================================

bool HP8114APulser::enableDisplay(bool enable)
{
    QString cmd = QString(":DISP %1").arg(enable ? "ON" : "OFF");
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Display" << (enable ? "enabled" : "disabled");
        return true;
    }
    return false;
}

// ============================================================================
// Self-test
// ============================================================================

bool HP8114APulser::performSelfTest()
{
    qDebug() << "HP8114A: Performing self-test...";
    int result = queryInt("*TST?");

    if (result == 0) {
        qDebug() << "HP8114A: Self-test passed";
        return true;
    } else {
        qWarning() << "HP8114A: Self-test failed with code:" << result;
        return false;
    }
}

// ============================================================================
// Double Pulse Mode
// ============================================================================

bool HP8114APulser::enableDoublePulse(bool enable)
{
    QString cmd = QString(":PULS:DOUB:STAT %1").arg(enable ? "ON" : "OFF");
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Double pulse mode" << (enable ? "enabled" : "disabled");
        return true;
    }
    return false;
}

bool HP8114APulser::setDoublePulseDelay(double delay_sec)
{
    if (!validateTiming(delay_sec)) {
        qWarning() << "HP8114A: Invalid double pulse delay:" << delay_sec;
        return false;
    }

    double delay_ns = delay_sec * 1e9;
    QString cmd = QString(":PULS:DOUB:DEL %1NS").arg(delay_ns);
    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Double pulse delay set to" << delay_ns << "ns";
        return true;
    }
    return false;
}

// ============================================================================
// Hold Mode Selection
// ============================================================================

bool HP8114APulser::setHoldMode(HoldMode mode)
{
    QString modeStr = (mode == HOLD_VOLTAGE) ? "VOLT" : "CURR";
    QString cmd = QString(":HOLD %1").arg(modeStr);

    if (sendCommand(cmd)) {
        qDebug() << "HP8114A: Hold mode set to" << modeStr;
        return true;
    }
    return false;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

QString HP8114APulser::buildCommand(const QString& scpiPath, const QString& value)
{
    if (value.isEmpty()) {
        return scpiPath;
    }
    return QString("%1 %2").arg(scpiPath, value);
}

QString HP8114APulser::buildQuery(const QString& scpiPath)
{
    if (!scpiPath.endsWith('?')) {
        return scpiPath + "?";
    }
    return scpiPath;
}

bool HP8114APulser::sendCommand(const QString& command)
{
    if (!isConnected()) {
        qWarning() << "HP8114A: Cannot send command - device not connected";
        return false;
    }

    // Ensure command ends with newline
    QString cmd = command;
    if (!cmd.endsWith('\n')) {
        cmd += '\n';
    }

    qDebug() << "HP8114A: Sending command:" << command;
    bool success = write(cmd);
    if (!success) {
        qWarning() << "HP8114A: Failed to send command:" << command;
        return false;
    }

    // Give device time to process the command
    QThread::msleep(50);

    return true;
}

QString HP8114APulser::queryCommand(const QString& query)
{
    if (!sendCommand(query)) {
        return QString();
    }

    QString response = read(256);
    return response.trimmed();
}

double HP8114APulser::queryDouble(const QString& query)
{
    QString response = queryCommand(query);
    if (response.isEmpty()) {
        return 0.0;
    }

    bool ok;
    double value = response.toDouble(&ok);
    if (!ok) {
        qWarning() << "HP8114A: Failed to parse double from response:" << response;
        return 0.0;
    }

    return value;
}

int HP8114APulser::queryInt(const QString& query)
{
    QString response = queryCommand(query);
    if (response.isEmpty()) {
        return 0;
    }

    bool ok;
    int value = response.toInt(&ok);
    if (!ok) {
        qWarning() << "HP8114A: Failed to parse int from response:" << response;
        return 0;
    }

    return value;
}

bool HP8114APulser::queryBool(const QString& query)
{
    QString response = queryCommand(query);
    if (response.isEmpty()) {
        return false;
    }

    response = response.trimmed();
    return (response == "1" || response.toUpper() == "ON");
}

void HP8114APulser::updateCachedValues()
{
    // Query all important parameters and update cache
    queryVoltageHigh();
    queryVoltageLow();
    queryVoltageAmplitude();
    queryVoltageBaseline();
    queryPulseWidth();
    queryPulsePeriod();
    isOutputEnabled();
}

bool HP8114APulser::validateVoltage(double voltage)
{
    // Basic validation - adjust limits based on your HP 8114A specifications
    const double MAX_VOLTAGE = 20.0; // Example limit
    const double MIN_VOLTAGE = -20.0; // Example limit

    if (voltage < MIN_VOLTAGE || voltage > MAX_VOLTAGE) {
        qWarning() << "HP8114A: Voltage out of range [" << MIN_VOLTAGE << "," << MAX_VOLTAGE << "]: " << voltage;
        return false;
    }

    return true;
}

bool HP8114APulser::validateTiming(double time_sec)
{
    if (time_sec < 0.0) {
        qWarning() << "HP8114A: Timing value cannot be negative:" << time_sec;
        return false;
    }

    // Basic validation - adjust limits based on your HP 8114A specifications
    const double MAX_TIME = 1.0;  // 1 second
    const double MIN_TIME = 1e-9; // 1 nanosecond

    if (time_sec < MIN_TIME || time_sec > MAX_TIME) {
        qWarning() << "HP8114A: Timing out of range [" << MIN_TIME << "," << MAX_TIME << "]: " << time_sec;
        return false;
    }

    return true;
}
