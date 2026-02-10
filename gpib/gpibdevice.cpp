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
        return true;
    }

    // ibdev(board, pad, sad, timeout, eot, eos)
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

void GpibDevice::clearDevice()
{
    if (m_connected && m_descriptor >= 0) {
        ibclr(m_descriptor);
        qDebug() << "GPIB device clear sent to address" << m_address;
    }
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

bool GpibDevice::checkServiceRequest()
{
    if (!m_connected) {
        return false;
    }

    return (ibsta & SRQI) != 0;
}

QString GpibDevice::readServiceRequestData(int maxLength)
{
    if (!m_connected) {
        setError("Device not connected");
        return QString();
    }

    char statusByte;
    ibrsp(m_descriptor, &statusByte);

    if (ibsta & ERR) {
        setError(QString("Failed to read service request status from address %1").arg(m_address));
        return QString();
    }

    if (statusByte & 0x10) {
        return read(maxLength);
    }

    return QString();
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

void GpibDevice::setError(const QString& error)
{
    m_lastError = error;
    qDebug() << "GpibDevice Error:" << error;
}
