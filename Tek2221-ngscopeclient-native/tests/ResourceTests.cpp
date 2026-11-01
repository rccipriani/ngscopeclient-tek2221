#include "Tek2221Resource.h"
#include <stdexcept>
#include <iostream>
int main()
{
    try
    {
        for(const std::string s:{"GPIB0::5::INSTR","GPIB0:5:INSTR","gpib0:5::instr","GPIB0::5:INSTR"})
            if(Tek2221::CanonicalGPIBResource(s)!="GPIB0::5::INSTR") throw std::runtime_error("Collapsed address not repaired");
        if(Tek2221::CanonicalGPIBResource("GPIB1:5:2:INSTR")!="GPIB1::5::2::INSTR") throw std::runtime_error("Secondary address changed");
        for(const std::string s:{"GPIB0:::5::INSTR","GPIB0::31::INSTR","GPIB0::5::31::INSTR","TCPIP0::5::INSTR","GPIB0::5::INSTR;INIT"})
        {
            bool bad=false; try { Tek2221::CanonicalGPIBResource(s); } catch(const std::exception&) { bad=true; }
            if(!bad) throw std::runtime_error("Invalid resource accepted: "+s);
        }
        std::cout<<"PASS plugin repairs collapsed GPIB address, preserves secondary address, rejects malformed/out-of-range resources\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; }
}
