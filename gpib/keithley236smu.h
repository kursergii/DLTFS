#ifndef KEITHLEY236SMU_H
#define KEITHLEY236SMU_H

#include "gpibdevice.h"
#include <QString>

/**
 * @brief Controller class for Keithley 236 Source Measure Unit
 *
 * Provides DC voltage sourcing for DLTFS measurements via GPIB.
 * The K236 applies a constant bias voltage and can briefly drop to zero
 * to create the measurement transient.
 *
 * Uses Keithley 236 legacy command format (not SCPI):
 *   F0,0X  - Voltage source, DC mode
 *   B<v>,0,0X - Set source voltage
 *   L<c>,0X - Set compliance
 *   N1X / N0X - Output ON / OFF
 *   J0X - Reset
 */
class Keithley236SMU : public GpibDevice
{
public:
    Keithley236SMU(int board, int address);
    ~Keithley236SMU();

    // Initialization
    bool initialize();
    bool reset();

    // Voltage source configuration
    bool setVoltageSource(double voltage, double currentCompliance);
    bool setSourceVoltage(double voltage);

    // Output control
    bool outputOn();
    bool outputOff();
    bool isOutputEnabled() const { return m_outputEnabled; }

    // Trigger: performs the zero-bias pulse (drop to 0V, wait, restore bias)
    bool trigger();

    // Measurement: trigger and read measured current (A)
    double readCurrent();

    // Cached getters
    double biasVoltage() const { return m_biasVoltage; }
    double compliance() const { return m_compliance; }
    double zeroDuration() const { return m_zeroDuration; }
    void setZeroDuration(double seconds) { m_zeroDuration = seconds; }

    bool sendCommand(const QString& command);

private:

    double m_biasVoltage;
    double m_compliance;
    double m_zeroDuration;  // Duration of zero-bias period in seconds
    bool m_outputEnabled;
};

#endif // KEITHLEY236SMU_H
