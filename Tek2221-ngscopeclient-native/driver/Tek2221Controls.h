#ifndef Tek2221Controls_h
#define Tek2221Controls_h
#include "Tek2221Native.h"
#include <map>
#include <array>
namespace Tek2221 {
struct ControlSpec {
    std::string key, label, query, command, field;
    std::vector<std::string> choices;
    int minimum=0, maximum=0, step=1;
};
const std::vector<ControlSpec>& Controls();
const std::vector<std::pair<std::string,std::string>>& Actions();
std::string ReadControl(MessageIO& io,const std::string& key);
std::string WriteControl(MessageIO& io,const std::string& key,const std::string& value);
// Parse only known writable SET? statements. Never replay arbitrary text.
std::map<std::string,std::string> ParseSetup(const std::string& text);
std::string BuildSetup(const std::map<std::string,std::string>& values);
class ControlState {
public:
    std::array<std::string,10> slots;
    bool plotHold=false;
    std::string Execute(MessageIO& io,const std::string& action,const std::string& argument);
};
}
#endif
