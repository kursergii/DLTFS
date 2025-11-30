#include "hp8114apulser.h"
#include <QDebug>

HP8114APulser::HP8114APulser(int board, int address)
    : GpibDevice(board, address)
    , m_currentAmplitude(0.0)
    , m_currentWidth(0.0)
    , m_currentPeriod(0.0)
{
}

HP8114APulser::~HP8114APulser()
{
}

bool HP8114APulser::setExternalTrigger()
{
    // Set trigger source to external
    if (!write(":TRIG:SOUR EXT\n")) {
        return false;
    }

    // Set trigger mode to TRIGGER (not continuous)
    if (!write(":TRIG:MODE TRIG\n")) {
        return false;
    }

    // Set external trigger slope to positive (rising edge)
    if (!write(":TRIG:SLOP POS\n")) {
        return false;
    }

    // Turn output on
    if (!write(":OUTP ON\n")) {
        return false;
    }

    qDebug() << "HP8114A configured for external trigger (rising edge)";
    return true;
}

bool HP8114APulser::setPulseParameters(double amplitude, double width, double period)
{
    if (!setAmplitude(amplitude)) return false;
    if (!setPulseWidth(width)) return false;
    if (!setPulsePeriod(period)) return false;

    qDebug() << "HP8114A pulse parameters set: A=" << amplitude << "V, W=" << width << "s, P=" << period << "s";
    return true;
}

bool HP8114APulser::setAmplitude(double amplitude)
{
    QString cmd = QString(":VOLT %1V\n").arg(amplitude);
    if (!write(cmd)) {
        return false;
    }

    m_currentAmplitude = amplitude;
    qDebug() << "HP8114A amplitude set to" << amplitude << "V";
    return true;
}

bool HP8114APulser::setPulseWidth(double width)
{
    QString cmd = QString(":PULS:WIDT %1S\n").arg(width, 0, 'E');
    if (!write(cmd)) {
        return false;
    }

    m_currentWidth = width;
    qDebug() << "HP8114A pulse width set to" << (width * 1e6) << "us";
    return true;
}

bool HP8114APulser::setPulsePeriod(double period)
{
    QString cmd = QString(":PULS:PER %1\n").arg(period);
    if (!write(cmd)) {
        return false;
    }

    m_currentPeriod = period;
    qDebug() << "HP8114A pulse period set to" << period << "s";
    return true;
}

bool HP8114APulser::setTriggerMode(TriggerMode mode)
{
    QString trigMode;
    switch (mode) {
        case INTERNAL: trigMode = "INT"; break;
        case EXTERNAL: trigMode = "EXT"; break;
        case MANUAL: trigMode = "MAN"; break;
        default: return false;
    }

    QString cmd = QString(":TRIG:SOUR %1\n").arg(trigMode);
    return write(cmd);
}

bool HP8114APulser::setTriggerSlope(TriggerSlope slope)
{
    QString slopeStr = (slope == POSITIVE) ? "POS" : "NEG";
    QString cmd = QString(":TRIG:SLOP %1\n").arg(slopeStr);
    return write(cmd);
}

bool HP8114APulser::enableOutput(bool enable)
{
    QString cmd = QString(":OUTP %1\n").arg(enable ? "ON" : "OFF");
    if (!write(cmd)) {
        return false;
    }

    qDebug() << "HP8114A output" << (enable ? "enabled" : "disabled");
    return true;
}

bool HP8114APulser::disableOutput()
{
    return enableOutput(false);
}

double HP8114APulser::getAmplitude()
{
    return m_currentAmplitude;
}

double HP8114APulser::getPulseWidth()
{
    return m_currentWidth;
}

double HP8114APulser::getPulsePeriod()
{
    return m_currentPeriod;
}

void HP8114APulser::updateStoredValues()
{
    // Could query the device here if needed
    // For now, we rely on the cached values from set operations
}
