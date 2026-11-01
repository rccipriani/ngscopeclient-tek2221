#include "scopehal.h"
#include "Tek2221Oscilloscope.h"
#include "SCPIVISATransport.h"
extern "C" __declspec(dllexport) void PluginInit()
{
    AddTransportClass(SCPIVISATransport);
    AddDriverClass(Tek2221Oscilloscope);
    LogNotice("Tek2221 native VISA DLL plugin loaded (driver tek2221_native, transport visa).\n");
}
