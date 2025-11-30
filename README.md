# DLTFS - RF Impedance Measurement System

A Qt6-based GUI application for automated measurement and data acquisition using RF/microwave test equipment. DLTFS controls an HP 4291A RF Impedance Analyzer and HP 8114A Pulse Generator to characterize the temperature or time-dependent electrical properties of components at high frequencies.

## Features

- **Automated Measurements**: Control and collect data from HP 4291A Impedance Analyzer
- **Pulse Generation**: Configure HP 8114A Pulse Generator for test signal generation
- **Arduino Integration**: Serial communication for synchronized triggering
- **Real-time Visualization**: Interactive plots with QCustomPlot for measurement data
- **Multi-point Sequences**: Support for extended measurement sequences with intelligent batching
- **Flexible Configuration**: Support for multiple measurement formats (CP, CS, ZTD, YTD, ZR, YR)

## Project Structure

```
DLTFS/
├── gpib/                          # Hardware abstraction layer
│   ├── gpibdevice.h/.cpp          # Base GPIB device interface
│   ├── hp4291analyzer.h/.cpp       # HP 4291A analyzer wrapper
│   └── hp8114apulser.h/.cpp        # HP 8114A pulse generator wrapper
├── qcustomplot/                   # Plotting library
│   ├── qcustomplot.h/.cpp         # Interactive data visualization
├── raw/                            # Legacy and reference files
│   ├── code.txt                    # Implementation notes
│   ├── hp4291_measurement.cpp      # Legacy C++ implementation
│   └── hp4291_measurement.py       # Reference Python implementation
├── measurements.h/.cpp             # Measurement thread orchestration
├── mw.h/.cpp                       # Main window GUI controller
├── mw.ui                           # Qt Designer UI definition
├── main.cpp                        # Application entry point
└── CMakeLists.txt                  # Build configuration
```

## Hardware Requirements

- **HP 4291A RF Impedance Analyzer** (GPIB address 17)
- **HP 8114A Pulse Generator** (GPIB address 14)
- **GPIB Interface**: USB-GPIB adapter or GPIB controller card
- **Arduino**: For auxiliary triggering via serial connection
- **Linux System** with linux-gpib library installed

## Build Requirements

- **CMake** 3.5+
- **Qt6** with components:
  - Widgets
  - SerialPort
  - PrintSupport
- **C++17** compatible compiler (tested with Clang 18.1.3)
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
- **HP8114APulser**: Pulse generator control with trigger and output configuration

### Measurement Engine (`measurements.cpp`)

Asynchronous thread-based measurement coordinator that:
- Manages communication between pulser and analyzer
- Handles multi-point measurement sequences with batching
- Coordinates Arduino serial triggers
- Emits progress signals for GUI updates

### User Interface (`mw.h/cpp`, `mw.ui`)

Main window providing:
- Device connection and initialization
- Parameter configuration (frequency, amplitude, pulse width)
- Real-time plot visualization
- Measurement control and monitoring

## Usage

1. **Connect Hardware**: Ensure GPIB devices and Arduino are connected
2. **Configure Parameters**:
   - Set measurement frequency
   - Configure pulse amplitude and width
   - Set number of measurement points
3. **Start Measurement**: Click "Start" to begin automated data collection
4. **Monitor Progress**: Real-time plot updates show measurement progress
5. **Export Data**: Use File menu to save measurement results

## Development Notes

- The project uses Qt's meta-object compiler (MOC) and UI compiler (UIC) for automatic code generation
- Hardware abstraction separates GPIB communication from measurement logic
- Thread-based architecture prevents GUI blocking during long measurements
- Reference Python scripts in `raw/` folder demonstrate alternative measurement approaches using PyVISA

## Version

v0.1 - Early stage development

## License

[Specify your project license here]
