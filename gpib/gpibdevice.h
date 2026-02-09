#ifndef GPIBDEVICE_H
#define GPIBDEVICE_H

#include <QString>

#ifdef USE_GPIB
#include <gpib/ib.h>
#endif

class GpibDevice
{
public:
    GpibDevice(int board, int address);
    ~GpibDevice();

    bool connect();
    void disconnect();
    bool isConnected() const;

    QString queryIdentification();
    bool write(const QString& command);
    QString read(int maxLength = 256);

    // HP8114A Pulser specific methods
    bool setExternalTrigger();

    // Service Request detection
    bool checkServiceRequest();
    QString readServiceRequestData(int maxLength = 256);

    int getAddress() const { return m_address; }
    QString getLastError() const { return m_lastError; }

private:
    int m_board;
    int m_address;
    int m_descriptor;
    bool m_connected;
    QString m_lastError;

    void setError(const QString& error);
};

#endif // GPIBDEVICE_H
