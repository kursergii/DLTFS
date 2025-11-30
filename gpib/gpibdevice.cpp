#include "gpibdevice.h"
#include <QDebug>

GpibDevice::GpibDevice(int board, int address)
    : m_board(board)
    , m_address(address)
    , m_descriptor(-1)
    , m_connected(false)
{
}

GpibDevice::~GpibDevice()
{
    disconnect();
}

bool GpibDevice::connect()
{
    if (m_connected) {
        return true; // Already connected
    }

    // ibdev(board, pad, sad, timeout, eot, eos)
    // board = GPIB board number
    // pad = primary address
    // sad = 0 (no secondary address)
    // timeout = T3s (3 second timeout)
    // eot = 1 (assert EOI on write)
    // eos = 0 (disable EOS detection)
    m_descriptor = ibdev(m_board, m_address, 0, T3s, 1, 0);

    if (m_descriptor < 0) {
        setError(QString("Failed to connect to GPIB device at address %1").arg(m_address));
        return false;
    }

    m_connected = true;
    qDebug() << "Successfully connected to GPIB device at address" << m_address;
    return true;
}

void GpibDevice::disconnect()
{
    if (m_connected && m_descriptor >= 0) {
        ibonl(m_descriptor, 0);
        m_descriptor = -1;
        m_connected = false;
        qDebug() << "Disconnected from GPIB device at address" << m_address;
    }
}

bool GpibDevice::isConnected() const
{
    return m_connected;
}

QString GpibDevice::queryIdentification()
{
    if (!m_connected) {
        setError("Device not connected");
        return QString();
    }

    if (!write("*IDN?\n")) {
        return QString();
    }

    return read();
}

bool GpibDevice::write(const QString& command)
{
    if (!m_connected) {
        setError("Device not connected");
        return false;
    }

    QByteArray data = command.toUtf8();
    ibwrt(m_descriptor, data.data(), data.size());

    if (ibsta & ERR) {
        setError(QString("Failed to write to GPIB device at address %1").arg(m_address));
        return false;
    }

    return true;
}

QString GpibDevice::read(int maxLength)
{
    if (!m_connected) {
        setError("Device not connected");
        return QString();
    }

    char buffer[maxLength];
    ibrd(m_descriptor, buffer, maxLength - 1);

    if (ibsta & ERR) {
        setError(QString("Failed to read from GPIB device at address %1").arg(m_address));
        return QString();
    }

    buffer[ibcnt] = '\0';
    return QString::fromUtf8(buffer);
}

bool GpibDevice::setExternalTrigger()
{
    if (!m_connected) {
        setError("Device not connected");
        return false;
    }

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

bool GpibDevice::checkServiceRequest()
{
    if (!m_connected) {
        return false;
    }

    // Check if device has asserted Service Request (SRQ)
    // ibsta is the global status variable set by GPIB operations
    // SRQI bit indicates if this device requested service
    return (ibsta & SRQI) != 0;
}

QString GpibDevice::readServiceRequestData(int maxLength)
{
    if (!m_connected) {
        setError("Device not connected");
        return QString();
    }

    // Check Serial Poll Status Byte
    char statusByte;
    ibrsp(m_descriptor, &statusByte);

    if (ibsta & ERR) {
        setError(QString("Failed to read service request status from address %1").arg(m_address));
        return QString();
    }

    // If device has data available (typically bit 6 - MAV: Message Available)
    if (statusByte & 0x10) {  // Check for data available bit
        return read(maxLength);
    }

    return QString();
}

void GpibDevice::setError(const QString& error)
{
    m_lastError = error;
    qDebug() << "GpibDevice Error:" << error;
}
