#ifndef HP4291ANALYZER_H
#define HP4291ANALYZER_H

#include "gpibdevice.h"
#include <QString>

class HP4291Analyzer : public GpibDevice
{
public:
    HP4291Analyzer(int board, int address);
    ~HP4291Analyzer();

    // HP 4291A specific measurement setup
    bool setupForMeasurement(double frequency);
    bool setupTrigger(bool external = false);

    // Measurement format options
    enum MeasurementFormat {
        CP,      // Capacitance parallel, Dissipation factor
        CS,      // Capacitance series, Resistance series
        ZTD,     // Impedance magnitude, Phase, Dissipation
        YTD,     // Admittance magnitude, Phase, Dissipation
        ZR,      // Impedance real, Impedance imaginary
        YR       // Admittance real, Admittance imaginary
    };

    bool setMeasurementFormat(MeasurementFormat format);

    // Read measurement data
    struct MeasurementData {
        double value1;  // Primary measurement (e.g., Cp)
        double value2;  // Secondary measurement (e.g., D)
        bool valid;
    };

    MeasurementData readMeasurement(int pointNumber = 1);

    // Display and trace management
    bool setupDisplay(const double* xData, const double* yData, int numPoints);
    bool clearTrace();

private:
    QString formatToString(MeasurementFormat format);
};

#endif // HP4291ANALYZER_H
