#ifndef HP8114APULSER_H
#define HP8114APULSER_H

#include "gpibdevice.h"
#include <QString>

class HP8114APulser : public GpibDevice
{
public:
    HP8114APulser(int board, int address);
    ~HP8114APulser();

    // HP 8114A specific configuration
    bool setExternalTrigger();
    bool setPulseParameters(double amplitude, double width, double period);
    bool setAmplitude(double amplitude);
    bool setPulseWidth(double width);
    bool setPulsePeriod(double period);

    // Trigger configuration
    enum TriggerMode {
        INTERNAL,
        EXTERNAL,
        MANUAL
    };

    enum TriggerSlope {
        POSITIVE,
        NEGATIVE
    };

    bool setTriggerMode(TriggerMode mode);
    bool setTriggerSlope(TriggerSlope slope);

    // Output control
    bool enableOutput(bool enable = true);
    bool disableOutput();

    // Query current settings
    double getAmplitude();
    double getPulseWidth();
    double getPulsePeriod();

private:
    double m_currentAmplitude;
    double m_currentWidth;
    double m_currentPeriod;

    void updateStoredValues();
};

#endif // HP8114APULSER_H
