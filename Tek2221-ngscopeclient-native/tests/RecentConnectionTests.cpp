#include "Tek2221RecentConnection.h"
#include <stdexcept>
#include <iostream>
int main()
{
    try
    {
        for(const auto& path:std::vector<std::string>{"GPIB0::5::INSTR","GPIB0::5::2::INSTR","127.0.0.1:22210","[::1]:5025","TCPIP0::host::inst0::INSTR","C:\\captures\\scope", ""})
        {
            const auto history="Tek2221:tek2221:visa:"+path;
            auto fields=SplitRecentInstrumentConnection(history);
            if(fields.size()!=4 || fields[0]!="Tek2221" || fields[1]!="tek2221" || fields[2]!="visa" || fields[3]!=path)
                throw std::runtime_error("Recent address changed: "+path);
            auto roundtrip=fields[0]+":"+fields[1]+":"+fields[2]+":"+fields[3];
            if(roundtrip!=history) throw std::runtime_error("History roundtrip differs");
        }
        auto null=SplitRecentInstrumentConnection("Offline:demo:null");
        if(null.size()!=3 || null[2]!="null") throw std::runtime_error("Legacy null transport changed");
        auto malformed=SplitRecentInstrumentConnection("not a connection");
        if(malformed.size()!=1) throw std::runtime_error("Malformed input unexpectedly accepted");
        std::cout<<"PASS recent-instrument VISA double-colon/secondary address, LAN, IPv6, Windows path, empty/null path and roundtrip\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; }
}
