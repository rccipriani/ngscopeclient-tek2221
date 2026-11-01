// Included after ngscopeclient.h. No instrument I/O runs on the render thread.
#ifndef Tek2221Panel_h
#define Tek2221Panel_h
#include "Tek2221Oscilloscope.h"
#include <map>
#include <string>
#include <array>
#include <cstdio>
inline const char* Tek2221ChoiceName(const std::string& code)
{
    if(code=="SAM") return "Sample";
    if(code=="PEA") return "Peak Detect";
    if(code=="AVE") return "Average";
    if(code=="ACC") return "AccPeak";
    if(code=="DEF") return "Default";
    if(code=="CURS1") return "Cursor 1";
    if(code=="CURS2") return "Cursor 2";
    if(code=="HPG") return "HP-GL";
    if(code=="EPS7") return "Epson 7-bit";
    if(code=="EPS8") return "Epson 8-bit";
    if(code=="TJE") return "ThinkJet";
    return code.c_str();
}
inline void RenderTek2221StatusBox(const char* title,const char* id,const Tek2221::StatusEntry& entry,float rows)
{
    using Tek2221::StatusKind;
    auto color=entry.kind==StatusKind::Error?ImVec4(1.0f,0.45f,0.45f,1.0f):entry.kind==StatusKind::Success?ImVec4(0.55f,0.9f,0.65f,1.0f):ImVec4(0.9f,0.92f,0.96f,1.0f);
    ImGui::PushID(id);
    ImGui::TextUnformatted(title); ImGui::SameLine();
    if(ImGui::SmallButton("Copy")) ImGui::SetClipboardText(entry.text.c_str());
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(0.12f,0.14f,0.18f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,color);
    if(ImGui::BeginChild("output",ImVec2(0,ImGui::GetTextLineHeightWithSpacing()*rows),ImGuiChildFlags_Borders))
    {
        ImGui::PushStyleColor(ImGuiCol_Text,color); ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(entry.text.c_str());
        ImGui::PopTextWrapPos(); ImGui::PopStyleColor();
    }
    ImGui::EndChild(); ImGui::PopStyleColor(2); ImGui::PopID();
}
inline void RenderTek2221Panel(const std::shared_ptr<Oscilloscope>& scope)
{
    auto tek=std::dynamic_pointer_cast<Tek2221Oscilloscope>(scope);
    if(!tek || !tek->IsNative() || !ImGui::TreeNode("Tek 2221 controls (v0.4)")) return;
    ImGui::TextWrapped("Run / Single / Stop control PC transfers. STORE, SAVE/CONT, BOTH + ALT and analog knobs remain front-panel controls.");
    int paired=tek->GetPaired()?1:0;
    if(ImGui::Combo("Capture mode",&paired,"Live\0Paired (requires SAVE)\0")) tek->SetPaired(paired!=0);
    if(paired) ImGui::TextWrapped("Set BOTH + ALT, press SAVE, and leave controls unchanged during transfer. ALT sampling is sequential; this is a frozen pair, not simultaneous sampling.");
    bool xy=tek->GetXY();
    if(ImGui::Checkbox("Native XY (physical X-Y switch)",&xy)) tek->SetXY(xy);
    float delay=static_cast<float>(tek->GetCaptureDelay());
    if(ImGui::InputFloat("Delay after capture (seconds)",&delay,0.5f,1.0f,"%.1f")) tek->SetCaptureDelay(delay);
    ImGui::TextWrapped("Capture mode, XY selection, delay, probe factors and setup slots are retained when you save the ngscopeclient session.");
    ImGui::TextWrapped("CH1 / CH2 are acquisition memory. REF4 CH1 / CH2 read the stored reference. Signal / Max and Min are the envelope bounds; in Y or XY records they are equal. XY automatically splits one native record into CH1 (X) and CH2 (Y).");
    auto status=tek->StatusSnapshot();
    bool busy=tek->ControlBusy();
    if(!status.operation.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0f,0.8f,0.35f,1.0f));
        ImGui::TextWrapped("Working: %s",status.operation.c_str());
        ImGui::PopStyleColor();
    }
    RenderTek2221StatusBox("Capture status","capture-status",status.capture,3);
    RenderTek2221StatusBox("Last action result","action-status",status.action,4);
    RenderTek2221StatusBox("Last query result","query-status",status.query,3);
    ImGui::BeginDisabled(busy);
    auto values=tek->ControlValues();
    // Small edit buffers are per instrument node (ImGui ID already scoped by the caller).
    // Numeric controls submit only on Apply, preventing half-entered values from reaching the scope.
    struct Edits { std::map<std::string,int> numbers; std::map<std::string,std::string> lastReplies; int slot=1,row=16; std::array<char,41> message{}; };
    static std::map<std::string,Edits> edits;
    auto id=tek->GetTransportConnectionString()+":"+tek->m_nickname;
    auto& edit=edits[id];
    for(const std::string group:{"Acquisition","Cursors","Interface","Status","Legacy plot"})
    {
        if(!ImGui::TreeNode(group.c_str())) continue;
        if(group=="Status") ImGui::TextWrapped("Read-only values. Click Read to query; they are not polled continuously.");
        if(group=="Legacy plot") ImGui::TextWrapped("For a legacy hardware plotter, not an ngscopeclient screen copy. Start / Auto ON pauses waveform traffic until Abort plot.");
        for(const auto& f:Tek2221::Controls())
        {
            if(f.label.compare(0,group.size()+3,group+" / ")!=0) continue;
            ImGui::PushID(f.key.c_str());
            auto label=f.label.substr(group.size()+3);
            auto it=values.find(f.key);
            std::string current=it==values.end()?"Not read":it->second;
            ImGui::TextWrapped("%s: %s",label.c_str(),Tek2221ChoiceName(current));
            if(ImGui::SmallButton("Read")) tek->QueueControl("read:"+f.key);
            if(!f.command.empty())
            {
                ImGui::SameLine();
                if(!f.choices.empty())
                {
                    if(ImGui::BeginCombo("Set value",Tek2221ChoiceName(current)))
                    {
                        for(const auto& c:f.choices)
                        {
                            bool unavailable=f.key=="LONG" && c=="OFF";
                            ImGui::BeginDisabled(unavailable);
                            if(ImGui::Selectable(Tek2221ChoiceName(c),current==c)) tek->QueueControl("write:"+f.key,c);
                            ImGui::EndDisabled();
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    auto inserted=edit.numbers.emplace(f.key,f.minimum);
                    int& value=inserted.first->second;
                    if(it!=values.end() && edit.lastReplies[f.key]!=current)
                    {
                        try { value=std::stoi(current); } catch(...) {}
                        edit.lastReplies[f.key]=current;
                    }
                    ImGui::InputInt("Value",&value,f.step,f.step*4);
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Apply")) tek->QueueControl("write:"+f.key,std::to_string(value));
                }
            }
            ImGui::Separator(); ImGui::PopID();
        }
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Actions"))
    {
        ImGui::InputInt("Setup slot (1 to 10)",&edit.slot);
        ImGui::InputInt("CRT message row (1 to 16)",&edit.row);
        ImGui::InputText("CRT message (40 characters)",edit.message.data(),edit.message.size());
        ImGui::TextWrapped("Setup slots belong to the ngscopeclient session. Save the session to retain them. Restore changes writable digital settings only. Save/erase REF4 overwrites scope reference memory.");
        for(const auto& action:Tek2221::Actions())
        {
            std::string argument;
            if(action.first=="setupsave" || action.first=="setupload") argument=std::to_string(edit.slot);
            if(action.first=="message") argument=std::to_string(edit.row)+":"+edit.message.data();
            if(ImGui::Button(action.second.c_str())) tek->QueueControl("action:"+action.first,argument);
        }
        ImGui::TreePop();
    }
    ImGui::EndDisabled(); ImGui::TreePop();
}
#endif
