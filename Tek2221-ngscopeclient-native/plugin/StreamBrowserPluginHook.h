// Generic Windows Stream Browser extension hook. Include in one host translation unit.
#pragma once
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <cstring>
extern "C" __declspec(dllexport) unsigned NgscopeclientStreamPanelHookVersion();
extern "C" __declspec(dllexport) unsigned NgscopeclientStreamPanelHookVersion() { return 1; }
inline void RenderStreamBrowserPluginPanels(const std::shared_ptr<Oscilloscope>& scope)
{
    using Render = void (*)(unsigned, const char*, size_t, size_t, void*, void*, ImGuiMemAllocFunc, ImGuiMemFreeFunc, void*);
    static const auto renderers=[] {
        std::vector<Render> result;
        HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
        if(snapshot==INVALID_HANDLE_VALUE) return result;
        MODULEENTRY32W entry={}; entry.dwSize=sizeof(entry);
        if(Module32FirstW(snapshot,&entry)) do {
            auto address=GetProcAddress(entry.hModule,"NgscopeclientRenderStreamPanelV1");
            Render fn=nullptr; static_assert(sizeof(fn)==sizeof(address));
            std::memcpy(&fn,&address,sizeof(fn));
            if(fn) result.push_back(fn);
        } while(Module32NextW(snapshot,&entry));
        CloseHandle(snapshot); return result;
    }();
    ImGuiMemAllocFunc alloc; ImGuiMemFreeFunc free; void* user;
    ImGui::GetAllocatorFunctions(&alloc,&free,&user);
    for(auto render:renderers)
        render(1,IMGUI_VERSION,sizeof(ImGuiIO),sizeof(ImGuiStyle),ImGui::GetCurrentContext(),
            const_cast<void*>(static_cast<const void*>(&scope)),alloc,free,user);
}
#else
inline void RenderStreamBrowserPluginPanels(const std::shared_ptr<Oscilloscope>&) {}
#endif
