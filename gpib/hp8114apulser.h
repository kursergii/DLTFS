#ifndef HP8114APULSER_H
#define HP8114APULSER_H

#include "gpibdevice.h"
#include <QString>

/**
 * @brief Controller class for HP 8114A Pulse Generator
 *
 * This class provides a comprehensive interface to control the HP 8114A
 * pulse generator via GPIB using SCPI commands. It supports voltage mode
 * pulse generation with configurable timing, triggering, and output control.
 *
 * Key features:
 * - Voltage mode pulse generation (CURRent mode not supported via GPIB without Option 001)
 * - Flexible pulse timing control (width, period, frequency, duty cycle)
 * - Multiple trigger modes (internal, external, manual)
 * - Status monitoring and error handling
 * - IEEE 488.2 common commands support
 */
class HP8114APulser : public GpibDevice
{
public:
    HP8114APulser(int board, int address);
    ~HP8114APulser();

    // Initialization and reset
    bool initialize();
    bool reset();
    bool clearStatus();
    QString getIdentification();

    // Voltage Configuration (Primary mode for DLTFS)
    enum VoltageParameter {
        HIGH_LEVEL,      // Pulse high level (V)
        LOW_LEVEL,       // Pulse low level (V)
        AMPLITUDE,       // Pulse amplitude (HIGH - LOW)
        BASELINE         // Baseline voltage
    };

    bool setVoltageHigh(double voltage);
    bool setVoltageLow(double voltage);
    bool setVoltageAmplitude(double amplitude);
    bool setVoltageBaseline(double baseline);
    bool setVoltageLimits(double high_limit, double low_limit);
    bool enableVoltageLimits(bool enable = true);

    // Pulse Timing Configuration
    bool setPulseWidth(double width_sec);
    bool setPulsePeriod(double period_sec);
    bool setPulseFrequency(double freq_hz);
    bool setPulseDutyCycle(double duty_percent);
    bool setPulseDelay(double delay_sec);
    bool setTrailingEdgeDelay(double delay_sec);

    // Convenience method for common DLTFS setup
    bool setPulseParameters(double high_v, double low_v, double width_sec, double period_sec);

    // Trigger Configuration
    enum TriggerSource {
        IMMEDIATE,       // Free-running mode (IMMediate)
        EXTERNAL,        // External trigger input (EXTernal)
        MANUAL           // Manual trigger (MANual) - front panel or *TRG command
    };

    enum TriggerSlope {
        POSITIVE,        // Rising edge (POSitive)
        NEGATIVE,        // Falling edge (NEGative)
        EITHER           // Both edges (EITHer)
    };

    enum TriggerInhibitMode {
        INHIBIT_RISE,    // Inhibit on rising edge
        INHIBIT_FALL,    // Inhibit on falling edge
        INHIBIT_HIGH,    // Inhibit when high
        INHIBIT_LOW      // Inhibit when low
    };

    bool setTriggerSource(TriggerSource source);
    bool setTriggerSlope(TriggerSlope slope);
    bool setTriggerLevel(double level_v);
    bool setTriggerCount(int count);
    bool trigger();  // Manual trigger (*TRG)

    // Trigger Inhibit Control
    bool enableTriggerInhibit(bool enable = true);
    bool setTriggerInhibitMode(TriggerInhibitMode mode);

    // External Width Mode
    bool enableExternalWidth(bool enable = true);

    // Output Control
    bool enableOutput(bool enable = true);
    bool disableOutput();
    bool isOutputEnabled();

    // Output Impedance Configuration
    bool setInternalImpedance(double ohms);
    bool setExternalImpedance(double ohms);
    enum OutputPolarity {
        POLARITY_POSITIVE,
        POLARITY_NEGATIVE
    };
    bool setOutputPolarity(OutputPolarity polarity);

    // Query Methods (read actual values from device)
    double queryVoltageHigh();
    double queryVoltageLow();
    double queryVoltageAmplitude();
    double queryVoltageBaseline();
    double queryPulseWidth();
    double queryPulsePeriod();
    double queryPulseFrequency();
    QString queryTriggerSource();
    QString queryTriggerSlope();

    // Cached getter methods (use stored values)
    double getVoltageHigh() const { return m_voltageHigh; }
    double getVoltageLow() const { return m_voltageLow; }
    double getPulseWidth() const { return m_pulseWidth; }
    double getPulsePeriod() const { return m_pulsePeriod; }
    double getPulseFrequency() const { return m_pulseFrequency; }

    // Status and Error Handling
    bool checkOperationComplete();
    bool waitForOperationComplete(int timeout_ms = 5000);
    QString getSystemError();
    bool hasError();

    // Status Register Operations
    int readEventStatusRegister();
    bool setEventStatusEnable(int mask);
    int readStatusByte();

    // Memory Operations (save/recall settings)
    bool saveSettings(int location);  // 1-9
    bool recallSettings(int location); // 0-9 (0 = default)

    // Display Control
    bool enableDisplay(bool enable = true);

    // Self-test
    bool performSelfTest();

    // Double Pulse Mode
    bool enableDoublePulse(bool enable = true);
    bool setDoublePulseDelay(double delay_sec);

    // Hold Mode Selection (VOLT/CURR)
    enum HoldMode {
        HOLD_VOLTAGE,
        HOLD_CURRENT
    };
    bool setHoldMode(HoldMode mode);

private:
    // Cached parameter values
    double m_voltageHigh;
    double m_voltageLow;
    double m_voltageAmplitude;
    double m_voltageBaseline;
    double m_pulseWidth;
    double m_pulsePeriod;
    double m_pulseFrequency;
    double m_pulseDutyCycle;
    TriggerSource m_triggerSource;
    TriggerSlope m_triggerSlope;
    bool m_outputEnabled;

    // Helper methods
    QString buildCommand(const QString& scpiPath, const QString& value = QString());
    QString buildQuery(const QString& scpiPath);
    bool sendCommand(const QString& command);
    QString queryCommand(const QString& query);
    double queryDouble(const QString& query);
    int queryInt(const QString& query);
    bool queryBool(const QString& query);

    void updateCachedValues();
    bool validateVoltage(double voltage);
    bool validateTiming(double time_sec);
};

#endif // HP8114APULSER_H
