#ifndef Tek2221Status_h
#define Tek2221Status_h
#include <string>
namespace Tek2221 {
enum class StatusKind { Neutral, Success, Error };
struct StatusEntry {
    std::string text;
    StatusKind kind=StatusKind::Neutral;
};
// The owning driver serializes access with its cache mutex.
// Query/capture updates cannot overwrite a completed action result.
struct PanelStatus {
    StatusEntry capture={"No capture completed in this connection yet."};
    StatusEntry action={"No action completed yet."};
    StatusEntry query={"No explicit query completed yet."};
    std::string operation;
    void Begin(const std::string& label) { operation=label; }
    void Capture(const std::string& text,bool success)
    { capture={text,success?StatusKind::Success:StatusKind::Error}; }
    void Complete(bool isQuery,const std::string& label,const std::string& result,bool success)
    {
        auto& entry=isQuery?query:action;
        entry={label+"\n"+result,success?StatusKind::Success:StatusKind::Error};
        operation.clear();
    }
};
}
#endif
