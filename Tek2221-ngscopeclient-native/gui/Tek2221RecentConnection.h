// Preserve the opaque transport path in nickname:driver:transport:path history entries.
// VISA and IPv6 paths contain colons (including significant empty fields).
#ifndef Tek2221RecentConnection_h
#define Tek2221RecentConnection_h
#include <string>
#include <vector>
inline std::vector<std::string> SplitRecentInstrumentConnection(const std::string& text)
{
    std::vector<std::string> fields;
    size_t begin=0;
    for(size_t i=0;i<3;++i)
    {
        auto colon=text.find(':',begin);
        if(colon==std::string::npos)
        {
            fields.push_back(text.substr(begin));
            return fields;
        }
        fields.push_back(text.substr(begin,colon-begin));
        begin=colon+1;
    }
    fields.push_back(text.substr(begin));
    return fields;
}
#endif
