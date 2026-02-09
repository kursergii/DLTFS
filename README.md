# DLTFS - RF Impedance Measurement System

A Qt6-based GUI application for automated measurement and data acquisition using RF/microwave test equipment. DLTFS controls an HP 4291A RF Impedance Analyzer and Keithley 236 Source Measure Unit to characterize the temperature or time-dependent electrical properties of components at high frequencies.

## Features

- **Automated Measurements**: Control and collect data from HP 4291A Impedance Analyzer
- **DC Bias Control**: Configure Keithley 236 SMU for bias voltage with zero-bias pulse triggering
- **Real-time Visualization**: Interactive plots with QCustomPlot for measurement data
- **Multi-point Sequences**: Support for extended measurement sequences with intelligent batching
- **Flexible Configuration**: Support for multiple measurement formats (CP, CS, ZTD, YTD, ZR, YR)

## Project Structure

```
DLTFS/
├── gpib/                          # Hardware abstraction layer
│   ├── gpibdevice.h/.cpp          # Base GPIB device interface
│   ├── hp4291analyzer.h/.cpp       # HP 4291A analyzer wrapper
│   └── keithley236smu.h/.cpp       # Keithley 236 SMU wrapper
├── qcustomplot/                   # Plotting library
│   ├── qcustomplot.h/.cpp         # Interactive data visualization
├── raw/                            # Legacy and reference files
│   ├── code.txt                    # Implementation notes
│   ├── hp4291_measurement.cpp      # Legacy C++ implementation
│   └── hp4291_measurement.py       # Reference Python implementation
├── docs/                            # Hardware documentation
│   └── *.pdf                        # Official hardware manuals
├── measurements.h/.cpp             # Measurement thread orchestration
├── mw.h/.cpp                       # Main window GUI controller
├── mw.ui                           # Qt Designer UI definition
├── main.cpp                        # Application entry point
├── CMakeLists.txt                  # Build configuration
└── CMakePresets.json               # CMake presets
```

## Hardware Requirements

- **HP 4291A RF Impedance Analyzer** (GPIB address 17)
- **Keithley 236 Source Measure Unit** (GPIB address 15)
- **GPIB Interface**: USB-GPIB adapter or GPIB controller card
- **Linux System** with linux-gpib library installed

## Build Requirements

- **CMake** 3.5+
- **Qt6** with components:
  - Widgets
  - PrintSupport
- **C++23** compatible compiler (tested with Clang 18.1.3)
- **linux-gpib** library

### Building

```bash
cd DLTFS
mkdir build
cd build
cmake ..
make
```

## Architecture

### GPIB Hardware Abstraction Layer (`gpib/`)

The hardware layer provides a clean abstraction over the linux-gpib library:

- **GpibDevice**: Base class handling low-level GPIB communication (connection, reading/writing, error handling)
- **HP4291Analyzer**: Specialized interface for impedance measurements with support for multiple measurement formats
- **Keithley236SMU**: DC voltage source control for bias voltage and zero-bias pulse triggering

### Measurement Engine (`measurements.cpp`)

Asynchronous thread-based measurement coordinator that:
- Manages communication between SMU and analyzer
- Applies DC bias voltage via Keithley 236, triggers zero-bias pulse to create transient
- Handles multi-point measurement sequences with batching
- Emits progress signals for GUI updates

### User Interface (`mw.h/cpp`, `mw.ui`)

Main window providing:
- Device connection and initialization
- Parameter configuration (frequency, bias voltage, zero-bias duration)
- Real-time plot visualization
- Measurement control and monitoring

## Usage

1. **Connect Hardware**: Ensure GPIB devices are connected
2. **Configure Parameters**:
   - Set measurement frequency
   - Configure DC bias voltage and zero-bias duration
   - Set number of measurement points
3. **Start Measurement**: Click "Start" to begin automated data collection
4. **Monitor Progress**: Real-time plot updates show measurement progress
5. **Export Data**: Use Save Data button to save measurement results

## Development Notes

- The project uses Qt's meta-object compiler (MOC) and UI compiler (UIC) for automatic code generation
- Hardware abstraction separates GPIB communication from measurement logic
- Thread-based architecture prevents GUI blocking during long measurements
- Reference Python scripts in `raw/` folder demonstrate alternative measurement approaches using PyVISA

## Version

v0.1 - Early stage development

## License

[Specify your project license here]
