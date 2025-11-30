#!/usr/bin/env python3
"""
HP 4291B RF Impedance Analyzer Measurement Program
Collects impedance/capacitance measurements over time and displays results.
"""

import pyvisa
import time
import numpy as np

# GPIB Configuration
GPIB_ADDRESS = 17
INSTRUMENT_ID = f'GPIB0::{GPIB_ADDRESS}::INSTR'

# Constants
TINT = 0.5  # INTERVAL TIME (SEC)
NOP = 21   # SAMPLE POINTS
F = 1.0E8   # MEASUREMENT FREQ. 100MHZ

def initialize_instrument(rm):
    """Initialize connection to HP 4291 instrument."""
    try:
        hp4291 = rm.open_resource(INSTRUMENT_ID)
        hp4291.timeout = 5000  # 5 second timeout

        # Clear the instrument
        hp4291.write('*CLS')
        hp4291.write('*RST')

        print(f"Connected to: {hp4291.query('*IDN?').strip()}")
        return hp4291
    except pyvisa.errors.VisaIOError as e:
        print(f"Error connecting to instrument at GPIB {GPIB_ADDRESS}: {e}")
        raise

def setup_measurement(hp4291):
    """Configure measurement parameters."""
    hp4291.write(f"SENS:FREQ:SPAN 0;CENT {F}")
    hp4291.write("CALC:MATH:STAT OFF")
    hp4291.write("CALC:FORM CP")
    print("Measurement configuration complete")

def setup_trigger(hp4291):
    """Configure trigger settings."""
    hp4291.write("STAT:INST:ENAB 128")
    hp4291.write("*SRE 4")
    hp4291.write("TRIG:SOUR BUS")
    hp4291.write("INIT:CONT ON;")
    hp4291.write("TRIG:EVEN:TYPE POIN")
    print("Trigger configuration complete")

def collect_measurements(hp4291, nop, tint):
    """
    Collect impedance measurements over time.

    Args:
        hp4291: PyVISA instrument object
        nop: Number of sample points
        tint: Time interval between samples (seconds)

    Returns:
        tuple: (x_data, y_data) - Time and measurement arrays
    """
    x_data = np.zeros(nop)
    y_data = np.zeros(nop)

    # Prepare X-axis (time) data
    for i in range(nop):
        x_data[i] = i * tint

    print(f"Collecting {nop} measurement points...")
    t1 = time.time()

    for i in range(nop):
        try:
            # Clear and prepare for next measurement
            hp4291.write('*CLS;*OPC?')
            opc = hp4291.read()

            # Trigger measurement
            hp4291.write('*TRG')

            # Wait for measurement to complete
            hp4291.write('*OPC?')
            opc = hp4291.read()

            # Read measurement value (returns Cp,D - capacitance and dissipation factor)
            hp4291.write(f"TRAC:VAL? DTR,{i+1}")
            response = hp4291.read().strip()
            # Parse first value (Cp) from comma-separated response
            values = response.split(',')
            y_data[i] = float(values[0])

            # Display progress
            elapsed_time = (i) * tint
            print(f"Point {i+1:3d}/{nop}: {elapsed_time:7.1f} [SEC]", end='\r')
            print(x_data[i], y_data[i])

            # Maintain timing interval
            t2 = time.time()
            while t2 - t1 < tint:
                time.sleep(0.01)
                t2 = time.time()
            t1 = t2

        except pyvisa.errors.VisaIOError as e:
            print(f"\nError during measurement {i+1}: {e}")
            raise

    print("\nMeasurement collection complete")
    return x_data, y_data

def setup_display(hp4291, x_data, y_data, nop):
    """Configure display settings and send data to instrument."""
    # X-axis (time) configuration
    hp4291.write("DISP:TRAC18:X:UNIT 'SEC'")
    hp4291.write(f"DISP:TRAC18:X:LEFT {x_data[0]}")
    hp4291.write(f"DISP:TRAC18:X:RIGHT {x_data[nop-1]}")
    hp4291.write("DISP:TEXT35 'ELAPSE TIME'")

    # Y-axis (frequency) configuration
    hp4291.write("DISP:TRAC18:Y:UNIT 'F'")
    hp4291.write(f"DISP:TRAC18:Y:BOTT {np.min(y_data)}")
    hp4291.write(f"DISP:TRAC18:Y:TOP {np.max(y_data)}")
    hp4291.write("DISP:TEXT31 'Cp'")

    # Send data to instrument
    hp4291.write(f"TRAC:POIN TR18,{nop}")

    # Format arrays for GPIB transmission
    x_str = ','.join([str(x) for x in x_data])
    y_str = ','.join([str(y) for y in y_data])

    hp4291.write(f"TRAC TRX18,{x_str}")
    hp4291.write(f"TRAC TRY18,{y_str}")

    # Enable user trace and markers
    hp4291.write("DISP:TRAC18:STAT ON")
    hp4291.write("CALC:EVAL:ON 'TR18'")
    hp4291.write("CALC:EVAL:INT OFF")

    print("Display configuration complete")

def main():
    """Main program execution."""
    rm = pyvisa.ResourceManager()
    hp4291 = None

    try:
        # Initialize instrument
        hp4291 = initialize_instrument(rm)

        # Configure measurement and trigger
        setup_measurement(hp4291)
        setup_trigger(hp4291)

        # Collect measurements
        x_data, y_data = collect_measurements(hp4291, NOP, TINT)

        # Setup and send display configuration
        setup_display(hp4291, x_data, y_data, NOP)

        print(x_data)
        print(y_data)

        # Wait for user interaction
        input("\nMove marker or press [RETURN] to clear trace: ")

        # Clear the trace
        hp4291.write("DISP:TRAC18:CLE")

        print("Program complete")


    except Exception as e:
        print(f"Error: {e}")
    finally:
        if hp4291:
            hp4291.close()
        rm.close()

if __name__ == "__main__":
    main()
