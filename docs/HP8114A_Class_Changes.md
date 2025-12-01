# HP8114APulser Class Rewrite - Changes Summary

## Date
2025-12-01

## Overview
The HP8114APulser class has been completely rewritten based on the official HP 8114A SCPI command reference documentation. The new implementation provides comprehensive control over the pulse generator with proper SCPI command structure.

## Major Changes

### 1. Header File (hp8114apulser.h)

#### Added Features

**Voltage Configuration Methods**
- `setVoltageHigh(double)` - Set pulse high level using `:SOURce:VOLTage:HIGH`
- `setVoltageLow(double)` - Set pulse low level using `:SOURce:VOLTage:LOW`
- `setVoltageAmplitude(double)` - Set pulse amplitude using `:SOURce:VOLTage:AMPLitude`
- `setVoltageBaseline(double)` - Set baseline voltage using `:SOURce:VOLTage:BASeline`
- `setVoltageLimits(double, double)` - Configure voltage safety limits
- `enableVoltageLimits(bool)` - Enable/disable voltage limit protection

**Enhanced Timing Control**
- `setPulseFrequency(double)` - Set frequency using `:SOURce:FREQuency`
- `setPulseDutyCycle(double)` - Set duty cycle using `:SOURce:PULSe:DCYCle`
- `setPulseDelay(double)` - Set pulse delay using `:SOURce:PULSe:DELay`
- `setTrailingEdgeDelay(double)` - Set trailing edge delay

**Advanced Trigger Features**
- `setTriggerLevel(double)` - Set external trigger threshold
- `setTriggerCount(int)` - Set number of pulses per trigger
- `trigger()` - Send manual trigger using `*TRG`
- `enableTriggerInhibit(bool)` - Enable trigger inhibit functionality
- `setTriggerInhibitMode(TriggerInhibitMode)` - Configure inhibit mode
- `enableExternalWidth(bool)` - Enable external width mode

**Output Configuration**
- `isOutputEnabled()` - Query actual output state from device
- `setInternalImpedance(double)` - Configure internal impedance
- `setExternalImpedance(double)` - Configure expected external load
- `setOutputPolarity(OutputPolarity)` - Set output polarity (positive/negative)

**Query Methods** (reads from device, not cached)
- `queryVoltageHigh()` - Read actual HIGH voltage setting
- `queryVoltageLow()` - Read actual LOW voltage setting
- `queryVoltageAmplitude()` - Read actual amplitude
- `queryVoltageBaseline()` - Read actual baseline
- `queryPulseWidth()` - Read actual pulse width
- `queryPulsePeriod()` - Read actual period
- `queryPulseFrequency()` - Read actual frequency
- `queryTriggerSource()` - Read trigger source setting
- `queryTriggerSlope()` - Read trigger slope setting

**Status and Error Handling**
- `initialize()` - Proper initialization sequence with device reset
- `reset()` - IEEE 488.2 `*RST` command
- `clearStatus()` - IEEE 488.2 `*CLS` command
- `getIdentification()` - IEEE 488.2 `*IDN?` query
- `checkOperationComplete()` - Check if operation finished
- `waitForOperationComplete(int)` - Wait with timeout for completion
- `getSystemError()` - Query system error queue
- `hasError()` - Check if system has errors
- `readEventStatusRegister()` - Read ESR
- `setEventStatusEnable(int)` - Configure ESE mask
- `readStatusByte()` - Read STB

**Memory Operations**
- `saveSettings(int)` - Save to memory location 1-9
- `recallSettings(int)` - Recall from memory location 0-9

**Additional Features**
- `enableDisplay(bool)` - Control front panel display
- `performSelfTest()` - Run device self-test
- `enableDoublePulse(bool)` - Enable double pulse mode
- `setDoublePulseDelay(double)` - Configure double pulse timing
- `setHoldMode(HoldMode)` - Switch between VOLT/CURR mode

#### New Enums
- `TriggerSource`: IMMEDIATE, EXTERNAL, MANUAL (fixed naming from old INTERNAL)
- `TriggerSlope`: POSITIVE, NEGATIVE, EITHER (added EITHER option)
- `TriggerInhibitMode`: INHIBIT_RISE, INHIBIT_FALL, INHIBIT_HIGH, INHIBIT_LOW
- `OutputPolarity`: POLARITY_POSITIVE, POLARITY_NEGATIVE
- `HoldMode`: HOLD_VOLTAGE, HOLD_CURRENT

#### Removed/Deprecated
- `setExternalTrigger()` - Replaced by `setTriggerSource(EXTERNAL)`
- `setAmplitude()` - Replaced by `setVoltageAmplitude()` for clarity
- Old `TriggerMode` enum - Replaced by `TriggerSource`
- `getAmplitude()`, `getPulseWidth()`, `getPulsePeriod()` - Replaced by const versions and query methods

#### Enhanced Member Variables
```cpp
// Comprehensive cached state
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
```

#### New Helper Methods
```cpp
QString buildCommand(const QString& scpiPath, const QString& value);
QString buildQuery(const QString& scpiPath);
bool sendCommand(const QString& command);
QString queryCommand(const QString& query);
double queryDouble(const QString& query);
int queryInt(const QString& query);
bool queryBool(const QString& query);
void updateCachedValues();
bool validateVoltage(double voltage);
bool validateTiming(double time_sec);
```

### 2. Implementation File (hp8114apulser.cpp)

#### Key Improvements

**Proper SCPI Command Format**
- All commands now use correct SCPI hierarchy
- Commands use long form for readability (e.g., `:SOURce:VOLTage:HIGH` instead of `:VOLT`)
- Proper use of scientific notation for numeric values
- Consistent command termination with newlines

**Comprehensive Error Handling**
- Connection checks before all operations
- Parameter validation before sending to device
- Detailed debug/warning messages via qDebug/qWarning
- Error recovery mechanisms

**Initialization Sequence**
```cpp
initialize() {
    1. Check connection
    2. Clear status (*CLS)
    3. Reset device (*RST)
    4. Set voltage mode (HOLD VOLT)
    5. Query and cache initial parameters
}
```

**Query/Response Handling**
- Helper methods for parsing responses (double, int, bool)
- Automatic caching of queried values
- Fallback to cached values on query failure
- Response trimming and validation

**Parameter Validation**
- Voltage range checking (±20V default, adjustable)
- Timing range checking (1ns to 1s default, adjustable)
- Prevents invalid parameters from being sent to device

**Wait for Operation Complete**
- Proper implementation using QElapsedTimer
- Configurable timeout (default 5000ms)
- Non-blocking checks with 10ms sleep intervals

## Migration Guide

### Old Code → New Code

```cpp
// OLD: Basic setup
pulser->setExternalTrigger();
pulser->setPulseParameters(amplitude, width, period);
pulser->enableOutput();

// NEW: Equivalent setup with more control
pulser->initialize();  // Includes reset and configuration
pulser->setVoltageHigh(amplitude);
pulser->setVoltageLow(0.0);
pulser->setPulseWidth(width);
pulser->setPulsePeriod(period);
pulser->setTriggerSource(HP8114APulser::EXTERNAL);
pulser->setTriggerSlope(HP8114APulser::POSITIVE);
pulser->enableOutput();
```

```cpp
// OLD: Query cached values
double amp = pulser->getAmplitude();
double width = pulser->getPulseWidth();

// NEW: Option 1 - Query from device
double high = pulser->queryVoltageHigh();
double width = pulser->queryPulseWidth();

// NEW: Option 2 - Use cached values (faster)
double high = pulser->getVoltageHigh();
double width = pulser->getPulseWidth();
```

```cpp
// OLD: Trigger mode
pulser->setTriggerMode(HP8114APulser::EXTERNAL);

// NEW: Trigger source (better naming)
pulser->setTriggerSource(HP8114APulser::EXTERNAL);
```

### Typical DLTFS Setup Sequence

```cpp
// Create and connect device
HP8114APulser pulser(0, 10);  // Board 0, Address 10
if (!pulser.connect()) {
    // Handle error
    return;
}

// Initialize device
if (!pulser.initialize()) {
    // Handle error
    return;
}

// Configure pulse parameters for DLTFS
pulser.setVoltageHigh(1.0);      // 1V pulse
pulser.setVoltageLow(0.0);       // 0V baseline
pulser.setPulseWidth(1e-3);      // 1ms pulse
pulser.setPulseFrequency(1000);  // 1kHz repetition

// Configure triggering
pulser.setTriggerSource(HP8114APulser::EXTERNAL);
pulser.setTriggerSlope(HP8114APulser::POSITIVE);
pulser.setTriggerLevel(2.5);     // 2.5V threshold

// Enable output
pulser.enableOutput(true);

// Verify settings
qDebug() << "High:" << pulser.queryVoltageHigh();
qDebug() << "Width:" << pulser.queryPulseWidth();
qDebug() << "Freq:" << pulser.queryPulseFrequency();

// Check for errors
if (pulser.hasError()) {
    qWarning() << "Device error:" << pulser.getSystemError();
}
```

## Benefits

1. **Standards Compliance**: Fully compliant with SCPI and IEEE 488.2 standards
2. **Better Documentation**: Comprehensive Doxygen comments and inline documentation
3. **Error Handling**: Robust error checking and reporting
4. **Flexibility**: Separate methods for each parameter allow fine-grained control
5. **Query Support**: Can read actual device state, not just cached values
6. **Safety**: Parameter validation prevents sending invalid values
7. **Maintainability**: Well-organized code with clear sections and helper methods
8. **Extensibility**: Easy to add new features based on SCPI command structure

## Testing Recommendations

1. **Connection Test**: Verify device responds to `*IDN?`
2. **Initialization Test**: Test `initialize()` sequence
3. **Parameter Tests**:
   - Set and query voltage parameters
   - Set and query timing parameters
   - Verify values match expectations
4. **Trigger Tests**: Test all trigger modes and slopes
5. **Output Tests**: Enable/disable output, verify with oscilloscope
6. **Error Handling**: Test invalid parameters, disconnection scenarios
7. **Memory Tests**: Save and recall settings
8. **Performance**: Measure command latency, optimize if needed

## Known Limitations

1. Current mode requires Option 001 (not available via standard HP-IB)
2. Voltage/timing validation limits are conservative and may need adjustment
3. Some advanced SCPI features not yet implemented (status groups, etc.)

## Future Enhancements

1. Add comprehensive status reporting using status registers
2. Implement advanced pulse patterns
3. Add measurement capabilities if supported
4. Create GUI controls for all parameters
5. Add automated test sequences
6. Implement parameter presets for common DLTFS scenarios

## References

- HP 8114A User's Guide (docs/8114A--Users_Guide_commands.pdf)
- HP8114A Command Reference (docs/HP8114A_Command_Reference.md)
- SCPI-1999 Standard
- IEEE 488.2 Standard
