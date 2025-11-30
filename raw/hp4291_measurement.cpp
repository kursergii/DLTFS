#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <chrono>
#include <thread>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include <gpib/ib.h>

// GPIB Configuration
const int GPIB_BOARD = 0;
const int GPIB_ADDRESS = 17;

// Constants
const double TINT = 0.5;      // INTERVAL TIME (SEC)
const int NOP = 201;          // SAMPLE POINTS
const double F = 1.0E8;       // MEASUREMENT FREQ. 100MHZ

// Buffer size for GPIB communication
const int BUFFER_SIZE = 4096;

class HP4291Controller {
private:
    int gpibDevice;
    char buffer[BUFFER_SIZE];

public:
    HP4291Controller() : gpibDevice(-1) {}

    ~HP4291Controller() {
        if (gpibDevice >= 0) {
            ibclr(gpibDevice);
            ibonl(gpibDevice, 0);
        }
    }

    bool initialize() {
        // Open GPIB device
        gpibDevice = ibdev(GPIB_BOARD, GPIB_ADDRESS, 0, T10s, 1, 0);

        if (gpibDevice < 0) {
            std::cerr << "Error: Cannot open GPIB device at address " << GPIB_ADDRESS << std::endl;
            return false;
        }

        // Clear device
        ibclr(gpibDevice);

        // Query and display instrument ID
        if (!query("*IDN?")) {
            std::cerr << "Error: Cannot communicate with instrument" << std::endl;
            return false;
        }

        std::cout << "Connected to: " << buffer << std::endl;
        return true;
    }

    void write(const std::string& command) {
        if (gpibDevice < 0) return;

        ibwrt(gpibDevice, (void*)command.c_str(), command.length());

        if (ibsta & ERR) {
            std::cerr << "Error writing command: " << command << std::endl;
        }
    }

    bool query(const std::string& command) {
        write(command);

        std::memset(buffer, 0, BUFFER_SIZE);
        ibrd(gpibDevice, buffer, BUFFER_SIZE - 1);

        if (ibsta & ERR) {
            std::cerr << "Error reading response" << std::endl;
            return false;
        }

        // Null-terminate the buffer
        buffer[ibcnt] = '\0';
        return true;
    }

    void setupMeasurement() {
        write("*CLS");
        write("*RST");

        char cmd[256];
        std::sprintf(cmd, "SENS:FREQ:SPAN 0;CENT %.1E", F);
        write(cmd);

        write("CALC:MATH:STAT OFF");
        write("CALC:FORM CP");

        std::cout << "Measurement configuration complete" << std::endl;
    }

    void setupTrigger() {
        write("STAT:INST:ENAB 128");
        write("*SRE 4");
        write("TRIG:SOUR EXT");
        write("INIT:CONT ON;");
        write("TRIG:EVEN:TYPE POIN");

        std::cout << "Trigger configuration complete (external trigger)" << std::endl;
    }

    void collectMeasurements(std::vector<double>& x_data, std::vector<double>& y_data) {
        // Prepare X-axis (time) data
        for (int i = 0; i < NOP; ++i) {
            x_data[i] = i * TINT;
        }

        std::cout << "Collecting " << NOP << " measurement points..." << std::endl;

        auto t1 = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NOP; ++i) {
            // Clear and prepare for next measurement
            write("*CLS");

            // Arm the instrument for external trigger
            write("INIT");

            // Wait for external trigger event
            write("*OPC?");
            query("");  // Read response

            // Read measurement value
            char cmd[256];
            std::sprintf(cmd, "TRAC:VAL? DTR,%d", i + 1);
            if (query(cmd)) {
                y_data[i] = std::atof(buffer);
            }

            // Display progress
            double elapsed_time = i * TINT;
            std::cout << "Point " << std::setw(3) << (i + 1) << "/" << NOP
                      << ": " << std::fixed << std::setprecision(1) << elapsed_time
                      << " [SEC]\r" << std::flush;

            // Maintain timing interval
            auto t2 = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration<double>(t2 - t1).count();

            while (elapsed < TINT) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                t2 = std::chrono::high_resolution_clock::now();
                elapsed = std::chrono::duration<double>(t2 - t1).count();
            }

            t1 = t2;
        }

        std::cout << "\nMeasurement collection complete" << std::endl;
    }

    void setupDisplay(const std::vector<double>& x_data, const std::vector<double>& y_data) {
        char cmd[BUFFER_SIZE];

        // X-axis (time) configuration
        write("DISP:TRAC18:X:UNIT 'SEC'");

        std::sprintf(cmd, "DISP:TRAC18:X:LEFT %.6E", x_data[0]);
        write(cmd);

        std::sprintf(cmd, "DISP:TRAC18:X:RIGHT %.6E", x_data[NOP - 1]);
        write(cmd);

        write("DISP:TEXT35 'ELAPSE TIME'");

        // Y-axis (frequency) configuration
        write("DISP:TRAC18:Y:UNIT 'F'");

        auto min_y = *std::min_element(y_data.begin(), y_data.end());
        auto max_y = *std::max_element(y_data.begin(), y_data.end());

        std::sprintf(cmd, "DISP:TRAC18:Y:BOTT %.6E", min_y);
        write(cmd);

        std::sprintf(cmd, "DISP:TRAC18:Y:TOP %.6E", max_y);
        write(cmd);

        write("DISP:TEXT31 'Cp'");

        // Send number of points
        std::sprintf(cmd, "TRAC:POIN TR18,%d", NOP);
        write(cmd);

        // Send X data
        std::sprintf(cmd, "TRAC TRX18,");
        std::string x_cmd(cmd);
        for (int i = 0; i < NOP; ++i) {
            if (i > 0) x_cmd += ",";
            char val[32];
            std::sprintf(val, "%.6E", x_data[i]);
            x_cmd += val;
        }
        write(x_cmd);

        // Send Y data
        std::sprintf(cmd, "TRAC TRY18,");
        std::string y_cmd(cmd);
        for (int i = 0; i < NOP; ++i) {
            if (i > 0) y_cmd += ",";
            char val[32];
            std::sprintf(val, "%.6E", y_data[i]);
            y_cmd += val;
        }
        write(y_cmd);

        // Enable user trace and markers
        write("DISP:TRAC18:STAT ON");
        write("CALC:EVAL:ON 'TR18'");
        write("CALC:EVAL:INT OFF");

        std::cout << "Display configuration complete" << std::endl;
    }

    void clearTrace() {
        write("DISP:TRAC18:CLE");
    }
};

int main() {
    HP4291Controller controller;

    // Initialize instrument
    if (!controller.initialize()) {
        return 1;
    }

    // Configure measurement and trigger
    controller.setupMeasurement();
    controller.setupTrigger();

    // Collect measurements
    std::vector<double> x_data(NOP);
    std::vector<double> y_data(NOP);

    controller.collectMeasurements(x_data, y_data);

    // Setup and send display configuration
    controller.setupDisplay(x_data, y_data);

    // Wait for user interaction
    std::cout << "\nMove marker or press [RETURN] to clear trace: ";
    std::cin.ignore();

    // Clear the trace
    controller.clearTrace();

    std::cout << "Program complete" << std::endl;

    return 0;
}
