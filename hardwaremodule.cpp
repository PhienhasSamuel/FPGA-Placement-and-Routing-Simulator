#include "hardwaremodule.h"

HardwareModule::HardwareModule()
    : name(""),
      requiredLUTs(0),
      requiredDSPs(0),
      requiredBRAMs(0),
      priority(0)
{
}

HardwareModule::HardwareModule(const QString& name,
                               int luts,
                               int dsps,
                               int brams,
                               int priority)
    : name(name),
      requiredLUTs(luts),
      requiredDSPs(dsps),
      requiredBRAMs(brams),
      priority(priority)
{
}

QString HardwareModule::getName() const
{
    return name;
}

int HardwareModule::getLUTs() const
{
    return requiredLUTs;
}

int HardwareModule::getDSPs() const
{
    return requiredDSPs;
}

int HardwareModule::getBRAMs() const
{
    return requiredBRAMs;
}

int HardwareModule::getPriority() const
{
    return priority;
}

QChar HardwareModule::getSymbol() const
{
    if (name.isEmpty())
        return '?';

    return name.at(0).toUpper();
}