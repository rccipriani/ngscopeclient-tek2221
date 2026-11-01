#ifndef Tek2221Resource_h
#define Tek2221Resource_h
#include <regex>
#include <string>
#include <stdexcept>
namespace Tek2221 {
// Work around the stock recent-menu parser's empty-field collapse, without changing the host.
inline std::string CanonicalGPIBResource(const std::string& text)
{
    std::smatch m;
    if(!std::regex_match(text,m,std::regex("GPIB([0-9]+):{1,2}([0-9]+)(?::{1,2}([0-9]+))?:{1,2}INSTR",std::regex::icase)))
        throw std::runtime_error("Expected GPIB<board>::<primary>[::<secondary>]::INSTR");
    auto validAddress=[](const std::string& s) { return s.size()<=2 && std::stoul(s)<=30; };
    if(!validAddress(m[2]) || (m[3].matched && !validAddress(m[3]))) throw std::runtime_error("GPIB address must be 0..30");
    return "GPIB"+m[1].str()+"::"+m[2].str()+(m[3].matched?"::"+m[3].str():"")+"::INSTR";
}
}
#endif
