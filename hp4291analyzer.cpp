#include "hp4291analyzer.h"
#include <QDebug>

HP4291Analyzer::HP4291Analyzer(int board, int address)
    : GpibDevice(board, address)
{
}

HP4291Analyzer::~HP4291Analyzer()
{
}

bool HP4291Analyzer::setupForMeasurement(double frequency)
{
    qDebug() << "Configuring HP 4291A for measurement at" << frequency << "Hz";

    // Clear status
    if (!write("*CLS\n")) {
        return false;
    }

    // Set frequency (SPAN 0 = single point, CENT = center frequency)
    QString freqCmd = QString("SENS:FREQ:SPAN 0;CENT %1\n").arg(frequency, 0, 'E');
    if (!write(freqCmd)) {
        return false;
    }

    // Turn off math functions
    if (!write("CALC:MATH:STAT OFF\n")) {
        return false;
    }

    qDebug() << "HP 4291A configured successfully";
    return true;
}

bool HP4291Analyzer::setupTrigger(bool external)
{
    QString trigSource = external ? "EXT" : "BUS";
    QString cmd = QString("TRIG:SOUR %1\n").arg(trigSource);

    if (!write(cmd)) {
        return false;
    }

    // Enable continuous initiation
    if (!write("INIT:CONT ON\n")) {
        return false;
    }

    qDebug() << "HP 4291A trigger configured:" << (external ? "EXTERNAL" : "BUS");
    return true;
}

QString HP4291Analyzer::formatToString(MeasurementFormat format)
{
    switch (format) {
        case CP: return "CP";
        case CS: return "CS";
        case ZTD: return "ZTD";
        case YTD: return "YTD";
        case ZR: return "ZR";
        case YR: return "YR";
        default: return "CP";
    }
}

bool HP4291Analyzer::setMeasurementFormat(MeasurementFormat format)
{
    QString cmd = QString("CALC:FORM %1\n").arg(formatToString(format));
    if (!write(cmd)) {
        return false;
    }

    qDebug() << "Measurement format set to:" << formatToString(format);
    return true;
}

HP4291Analyzer::MeasurementData HP4291Analyzer::readMeasurement(int pointNumber)
{
    MeasurementData data;
    data.valid = false;

    // Trigger measurement
    if (!write("*CLS;*OPC?\n")) {
        return data;
    }
    read(); // Read OPC response

    if (!write("*TRG\n")) {
        return data;
    }

    // Wait for completion
    if (!write("*OPC?\n")) {
        return data;
    }
    read(); // Read OPC response

    // Read measurement data
    QString cmd = QString("TRAC:VAL? DTR,%1\n").arg(pointNumber);
    if (!write(cmd)) {
        return data;
    }

    QString response = read(1024);
    if (response.isEmpty()) {
        return data;
    }

    // Parse comma-separated values
    QStringList values = response.trimmed().split(',');

    if (values.size() >= 2) {
        bool ok1, ok2;
        data.value1 = values[0].toDouble(&ok1);
        data.value2 = values[1].toDouble(&ok2);
        data.valid = ok1 && ok2;
    } else if (values.size() == 1) {
        bool ok;
        data.value1 = values[0].toDouble(&ok);
        data.value2 = 0.0;
        data.valid = ok;
    }

    return data;
}

bool HP4291Analyzer::setupDisplay(const double* xData, const double* yData, int numPoints)
{
    if (numPoints <= 0 || !xData || !yData) {
        return false;
    }

    // X-axis configuration
    write("DISP:TRAC18:X:UNIT 'SEC'\n");
    write(QString("DISP:TRAC18:X:LEFT %1\n").arg(xData[0], 0, 'E'));
    write(QString("DISP:TRAC18:X:RIGHT %1\n").arg(xData[numPoints - 1], 0, 'E'));
    write("DISP:TEXT35 'ELAPSE TIME'\n");

    // Y-axis configuration
    write("DISP:TRAC18:Y:UNIT 'F'\n");

    // Find min/max
    double minY = yData[0];
    double maxY = yData[0];
    for (int i = 1; i < numPoints; ++i) {
        if (yData[i] < minY) minY = yData[i];
        if (yData[i] > maxY) maxY = yData[i];
    }

    write(QString("DISP:TRAC18:Y:BOTT %1\n").arg(minY, 0, 'E'));
    write(QString("DISP:TRAC18:Y:TOP %1\n").arg(maxY, 0, 'E'));
    write("DISP:TEXT31 'Cp'\n");

    // Send number of points
    write(QString("TRAC:POIN TR18,%1\n").arg(numPoints));

    // Format and send X data
    QString xCmd = "TRAC TRX18,";
    for (int i = 0; i < numPoints; ++i) {
        if (i > 0) xCmd += ",";
        xCmd += QString::number(xData[i], 'E', 6);
    }
    write(xCmd + "\n");

    // Format and send Y data
    QString yCmd = "TRAC TRY18,";
    for (int i = 0; i < numPoints; ++i) {
        if (i > 0) yCmd += ",";
        yCmd += QString::number(yData[i], 'E', 6);
    }
    write(yCmd + "\n");

    // Enable trace display
    write("DISP:TRAC18:STAT ON\n");
    write("CALC:EVAL:ON 'TR18'\n");
    write("CALC:EVAL:INT OFF\n");

    qDebug() << "Display configured with" << numPoints << "points";
    return true;
}

bool HP4291Analyzer::clearTrace()
{
    return write("DISP:TRAC18:CLE\n");
}
