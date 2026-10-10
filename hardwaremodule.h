#ifndef HARDWAREMODULE_H
#define HARDWAREMODULE_H

#include <QString>

class HardwareModule
{
public:
    HardwareModule();
    
    HardwareModule(const QString& name,
                   int luts,
                   int dsps,
                   int brams,
                   int priority);

    QString getName() const;
    int getLUTs() const;
    int getDSPs() const;
    int getBRAMs() const;
    int getPriority() const;

    QChar getSymbol() const;

private:
    QString name;
    int requiredLUTs;
    int requiredDSPs;
    int requiredBRAMs;
    int priority;
};

#endif // HARDWAREMODULE_H