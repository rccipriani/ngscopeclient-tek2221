#include "scopehal.h"
#include "imgui.h"
#include "../gui/Tek2221Panel.h"
#include <cstring>
extern "C" __declspec(dllexport) void NgscopeclientRenderStreamPanelV1(
    unsigned abi, const char* version, size_t ioSize, size_t styleSize, void* context,
    void* scope, ImGuiMemAllocFunc alloc, ImGuiMemFreeFunc free, void* user)
{
    if(abi!=1 || !version || std::strcmp(version,IMGUI_VERSION)!=0 ||
        ioSize!=sizeof(ImGuiIO) || styleSize!=sizeof(ImGuiStyle))
    {
        static bool reported=false;
        if(!reported) { LogError("Tek2221 sidebar: incompatible host ImGui build; rebuild DLL against host sources.\n"); reported=true; }
        return;
    }
    ImGui::SetAllocatorFunctions(alloc,free,user);
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));
    try { RenderTek2221Panel(*static_cast<std::shared_ptr<Oscilloscope>*>(scope)); }
    catch(const std::exception& e) { LogError("Tek2221 sidebar: %s\n",e.what()); }
}
