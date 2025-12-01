# HP 8114A Pulse Generator - Command Reference

## Overview
This document provides a reference for controlling the HP 8114A Pulse Generator via GPIB interface for the DLTFS (Deep Level Transient Fourier Spectroscopy) measurement system.

## Connection Details
- **Interface**: GPIB (IEEE 488.2)
- **Protocol**: SCPI (Standard Commands for Programmable Instruments)
- **Device**: HP 8114A Pulse/Pattern Generator

---

## IEEE 488.2 Common Commands

### Basic Device Control

| Command | Description | Usage Example |
|---------|-------------|---------------|
| `*CLS` | Clear the status structure | `*CLS` |
| `*IDN?` | Read instrument identification string | `*IDN?` |
| `*RST` | Reset to standard settings | `*RST` |
| `*OPC` | Generate Operation Complete message | `*OPC` |
| `*OPC?` | Query Operation Complete status | `*OPC?` |
| `*WAI` | Wait until all pending actions complete | `*WAI` |
| `*TRG` | Trigger the instrument | `*TRG` |
| `*TST?` | Execute self-test | `*TST?` |

### Memory and Configuration

| Command | Parameter | Description |
|---------|-----------|-------------|
| `*SAV` | `<1-9>` | Save complete instrument setting to memory |
| `*RCL` | `<0-9>` | Recall complete instrument setting from memory |

### Status Registers

| Command | Parameter | Description |
|---------|-----------|-------------|
| `*ESE` | `<0-255>` | Set Event Status Register Mask |
| `*ESE?` | - | Read Event Status Register Mask |
| `*ESR?` | - | Read Event Status Register |
| `*SRE` | `<0-255>` | Set Service Request Enable Mask |
| `*SRE?` | `<0-255>` | Read Service Request Enable Mask |
| `*STB?` | - | Read Status Byte |

---

## SCPI Commands for DLTFS Application

### Critical Commands for Pulse Generation

#### 1. Pulse Output Control

```
:OUTPut[:STATe] {ON|OFF|1|0}
:OUTPut[:STATe]?
```
**Purpose**: Enable/disable the pulse output
**DLTFS Usage**: Turn on pulse output before measurement, turn off after

#### 2. Voltage Mode Commands

**Important Note**: The CURRent and VOLTage subsystems cannot be used simultaneously. Use `:HOLD` command to select between them.

##### Amplitude and Levels
```
:SOURce:VOLTage:AMPLitude <value>
:SOURce:VOLTage:AMPLitude?

:SOURce:VOLTage:BASeline <value>
:SOURce:VOLTage:BASeline?

:SOURce:VOLTage:HIGH <value>
:SOURce:VOLTage:HIGH?

:SOURce:VOLTage:LOW <value>
:SOURce:VOLTage:LOW?
```
**DLTFS Usage**:
- Set pulse amplitude for capacitance transient measurements
- Configure baseline voltage (typically 0V or reverse bias)
- Set HIGH level for pulse peak
- Set LOW level for pulse baseline

##### Voltage Limits
```
:SOURce:VOLTage:LIMit:HIGH
:SOURce:VOLTage:LIMit:HIGH?

:SOURce:VOLTage:LIMit:LOW
:SOURce:VOLTage:LIMit:LOW?

:SOURce:VOLTage:LIMit:STATe {ON|OFF|1|0}
```
**Purpose**: Set maximum and minimum voltage limits for safety

#### 3. Pulse Timing Control

##### Frequency
```
:SOURce:FREQuency <value>
:SOURce:FREQuency?
```
**DLTFS Usage**: Set pulse repetition rate (typically related to temperature scan rate and time constants)

##### Pulse Width
```
:SOURce:PULSe:WIDTh <value>
:SOURce:PULSe:WIDTh?
```
**DLTFS Usage**: Define filling pulse width (must be long enough to fill traps)

##### Period
```
:SOURce:PULSe:PERiod <value>
:SOURce:PULSe:PERiod?
```
**DLTFS Usage**: Set pulse period (inverse of frequency)

##### Duty Cycle
```
:SOURce:PULSe:DCYCle <value>
:SOURce:PULSe:DCYCle?
```
**DLTFS Usage**: Alternative way to set pulse width as percentage of period

##### Delay
```
:SOURce:PULSe:DELay <value>
:SOURce:PULSe:DELay?
```
**Purpose**: Set channel delay (to leading edge)

##### Trailing Edge Delay
```
:SOURce:PULSe:TrailingTDELay <value>
:SOURce:PULSe:TrailingTDELay?
```
**Purpose**: Set trailing edge delay

#### 4. Hold Mode Selection

```
:SOURce:HOLD {VOLT|CURR}
```
**Purpose**: Switch between VOLTage and CURRent command subtrees
**Note**: Standard HP 8114A cannot program current via HP-IB bus. Use Option 001 if current programming is needed.

#### 5. Trigger Configuration

```
:TRIGger[:SBQuence]:STARt

:TRIGger:COUNt <value>
:TRIGger:COUNt?
```
**DLTFS Usage**:
- Start triggered pulse generation
- Set number of pulses to generate in burst mode

##### Trigger Source
```
:TRIGger:SOURce {IMMediate|EXTernal|MANual}
:TRIGger:SOURce?
```
**Options**:
- `IMMediate`: Free-running mode
- `EXTernal`: External trigger input
- `MANual`: Manual trigger (front panel or `*TRG` command)

##### Trigger Level and Slope
```
:TRIGger:LEVel <value>
:TRIGger:LEVel?

:TRIGger:SLOPe {POSitive|NEGative|EITHer}
:TRIGger:SLOPe?
```
**Purpose**: Set external trigger threshold and edge sensitivity

##### Trigger Inhibit
```
:TRIGger[:INHibit][:STATe] {ON|OFF|1|0}
:TRIGger[:INHibit]:MODE {RISe|FALL|HIGH|LOW}
:TRIGger[:INHibit]:INPut {RESel|SET}
```
**Purpose**: Enable/disable trigger inhibit functionality

##### External Width Mode
```
:TRIGger[:EWIDth]:STATe {ON|OFF|1|0}
```
**Purpose**: Enable/disable external width mode

---

## Status and System Commands

### Status Model

```
:STATus:OPERation[:EVENt]?
:STATus:OPERation:CONDition?
:STATus:OPERation:ENABle <Numeric>
:STATus:OPERation:NTRansition <Numeric>
:STATus:OPERation:PTRansition <Numeric>
:STATus:PRESet
```
**Purpose**: Monitor operation status and configure status reporting

### Questionable Status

```
:STATus:QUEStionable[:EVENt]?
:STATus:QUEStionable:CONDition?
:STATus:QUEStionable:ENABle <Numeric>
:STATus:QUEStionable:NTRansition <Numeric>
:STATus:QUEStionable:PTRansition <Numeric>
```
**Purpose**: Monitor questionable conditions

### System Commands

```
:SYSTem:ERRor?
:SYSTem:KEY <Numeric>
:SYSTem:KEY?
:SYSTem:VERSion?
:SYSTem:SECurity[:STATe] {ON|OFF}
:SYSTem:SET?
```

---

## Implementation Recommendations for DLTFS

### 1. Initialization Sequence

```cpp
// Recommended initialization sequence for DLTFS measurements
*RST                                    // Reset to known state
*CLS                                    // Clear status
:SOURce:HOLD VOLT                      // Select voltage mode
:SOURce:VOLTage:HIGH 1.0               // Set high level (e.g., 1V)
:SOURce:VOLTage:LOW 0.0                // Set low level (0V)
:SOURce:PULSe:WIDTh 1E-3               // Set pulse width (e.g., 1ms)
:SOURce:FREQuency 1000                 // Set frequency (e.g., 1kHz)
:OUTPut ON                             // Enable output
```

### 2. Typical DLTFS Pulse Parameters

| Parameter | Typical Range | Notes |
|-----------|---------------|-------|
| Pulse Width | 100 µs - 10 ms | Must be sufficient to fill traps |
| Frequency | 10 Hz - 10 kHz | Related to rate window |
| Amplitude | 0.1 V - 5 V | Depends on device and measurement |
| Baseline | 0 V or reverse bias | Depends on device type |

### 3. Measurement Cycle

```
1. Set pulse parameters (width, amplitude, frequency)
2. Enable output (:OUTPut ON)
3. Wait for stabilization
4. Trigger capacitance measurement system
5. Acquire transient data
6. Process data for DLTS signal
7. Repeat for different temperatures
```

### 4. Safety Considerations

- Always set voltage limits before enabling output
- Verify pulse parameters before applying to device
- Monitor device current to prevent damage
- Use appropriate coupling and impedance matching

### 5. Query vs. Set Commands

- Commands ending with `?` are queries (read-only)
- Commands without `?` are set commands (write)
- Always use `*OPC?` or `*WAI` after critical commands to ensure completion

---

## Error Handling

### Common Errors
1. **Timeout**: Device not responding - check GPIB address and connection
2. **Command Error**: Invalid syntax or parameter - verify command format
3. **Execution Error**: Invalid state or parameter value - check device limits
4. **Query Error**: Query interrupted or buffer full - flush buffer before query

### Status Checking
```cpp
// Always check for errors after command sequences
:SYSTem:ERRor?    // Returns error code and message
*ESR?             // Check Event Status Register
```

---

## Memory Card Commands (Optional)

If memory card operations are needed:

```
:MMEMory:CATalog? [A:]                 // Read directory
:MMEMory:CDIRectory [<name>]           // Change directory
:MMEMory:COPY <source>,[A:],<dest>,[A:] // Copy file
:MMEMory:DELete <name>,[A:]            // Delete file
:MMEMory:INITialize [A:],[DOS]]        // Initialize memory card
:MMEMory:LOAD <n>,<name>,[A:]          // Load file from memory card
:MMEMory:STORe <n>,<name>,[A:]         // Store memory n to card
```

---

## Display Commands (Optional)

```
:DISPlay[:WINDow][:STATe] {ON|OFF|1|0}
```
**Purpose**: Control front panel display (may speed up operations when OFF)

---

## Integration with Qt Application

### Recommended Class Structure

```cpp
class HP8114AController {
public:
    // Initialization
    bool initialize();
    bool reset();

    // Pulse configuration
    bool setPulseVoltage(double high, double low);
    bool setPulseWidth(double width_sec);
    bool setPulseFrequency(double freq_hz);
    bool setPulsePeriod(double period_sec);

    // Output control
    bool enableOutput(bool enable);

    // Trigger control
    bool setTriggerSource(TriggerSource source);
    bool trigger();

    // Query functions
    QString getIdentification();
    double getPulseWidth();
    double getPulseFrequency();
    bool isOutputEnabled();

    // Error handling
    QString getLastError();
    bool checkOperationComplete();

private:
    GPIBDevice* m_device;
    QString m_lastError;
};
```

### Thread Safety
- All GPIB commands should be executed in a separate thread
- Use Qt signals/slots for communication with GUI
- Implement proper mutex locking for shared resources

---

## Testing and Validation

### Test Sequence
1. **Connection Test**: `*IDN?` should return device identification
2. **Reset Test**: `*RST` followed by parameter queries
3. **Output Test**: Set known parameters and verify with oscilloscope
4. **Timing Test**: Verify pulse width and frequency accuracy
5. **Trigger Test**: Test external and manual trigger modes

### Validation Checklist
- [ ] Device responds to `*IDN?`
- [ ] Reset command works (`*RST`)
- [ ] Can set and read voltage parameters
- [ ] Can set and read timing parameters
- [ ] Output enable/disable works
- [ ] Trigger functionality verified
- [ ] Error handling works correctly
- [ ] No memory leaks in GPIB communication
- [ ] Thread-safe operation confirmed

---

## References

- HP 8114A User's Guide
- IEEE 488.2 Standard
- SCPI Command Reference
- DLTFS Measurement Theory

---

## Notes

- Commands are case-insensitive
- Multiple commands can be sent on one line separated by semicolons
- Short form and long form commands are equivalent (e.g., `:OUTP` = `:OUTPut`)
- Numeric parameters can use scientific notation (e.g., 1E-3 = 0.001)
- Query commands always end with `?`

## Future Enhancements

1. Add automatic parameter optimization for different trap types
2. Implement pulse sequence programming for complex measurements
3. Add logging and data export functionality
4. Create GUI controls for real-time pulse parameter adjustment
5. Implement automated calibration routines
