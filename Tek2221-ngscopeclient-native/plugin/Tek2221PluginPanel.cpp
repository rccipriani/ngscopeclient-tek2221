#include "scopehal.h"
#include "Tek2221Oscilloscope.h"
#include "Tek2221PluginPanel.h"
#include <shellapi.h>
#include <atomic>
#include <thread>
#include <sstream>
#include <locale>
#include <stdexcept>
#include <cmath>

namespace {
constexpr UINT TrayMessage=WM_APP+31;
constexpr int Control=100,Value=101,Read=102,Write=103,Action=104,Execute=105,
    Slot=106,Row=107,Message=108,LocalApply=109,LocalRead=110;
std::wstring Wide(const std::string& s)
{
    int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);
    std::wstring w(n,L' '); if(n) MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n); return w;
}
std::string Narrow(const std::wstring& w)
{
    int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);
    std::string s(n,' '); if(n) WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr); return s;
}
std::string Text(HWND window)
{
    auto n=GetWindowTextLengthW(window); std::wstring w(size_t(n)+1,L'\0');
    GetWindowTextW(window,w.data(),n+1); w.resize(size_t(n)); return Narrow(w);
}
void SetText(HWND window,const std::string& text)
{
    std::string display;
    for(size_t i=0;i<text.size();++i) { if(text[i]=='\n' && (i==0 || text[i-1]!='\r')) display+='\r'; display+=text[i]; }
    if(Text(window)==display) return;
    auto w=Wide(display); SetWindowTextW(window,w.c_str());
}
double Number(const std::string& s)
{
    std::istringstream in(s); in.imbue(std::locale::classic()); double n; std::string extra;
    if(!(in>>n) || (in>>extra) || !std::isfinite(n)) throw std::runtime_error("Enter a finite number");
    return n;
}
}
struct Tek2221PluginPanel::Impl {
    Tek2221Oscilloscope* scope;
    std::atomic<bool> stopping{false}; std::thread thread;
    HWND window=nullptr,control=nullptr,value=nullptr,apply=nullptr,actions=nullptr,
        slot=nullptr,row=nullptr,message=nullptr,paired=nullptr,xy=nullptr,delay=nullptr,
        probe1=nullptr,probe2=nullptr,activity=nullptr,capture=nullptr,lastAction=nullptr,lastQuery=nullptr;
    NOTIFYICONDATAW tray{}; HINSTANCE module=nullptr;
    std::string pendingRead,uiError;
    explicit Impl(Tek2221Oscilloscope* s) : scope(s) { thread=std::thread([this]{Run();}); }
    ~Impl() { stopping=true; if(thread.joinable()) thread.join(); }
    HWND Item(const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id=0)
    {
        auto h=CreateWindowExW(type==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,type,text,
            WS_CHILD|WS_VISIBLE|style,x,y,width,height,window,reinterpret_cast<HMENU>(INT_PTR(id)),module,nullptr);
        SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE); return h;
    }
    HWND Label(const wchar_t* text,int y) { return Item(L"STATIC",text,0,12,y,750,18); }
    HWND Edit(int x,int y,int width,int height,int id,bool readOnly=false)
    {
        return Item(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL|(readOnly?(ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL):0),x,y,width,height,id);
    }
    void Init()
    {
        Label(L"Local capture settings (hardware SAVE/CONT and analog knobs stay on the scope)",10);
        paired=Item(L"BUTTON",L"Paired (SAVE required)",BS_AUTOCHECKBOX|WS_TABSTOP,12,33,190,22);
        xy=Item(L"BUTTON",L"Native XY (physical X-Y switch)",BS_AUTOCHECKBOX|WS_TABSTOP,210,33,260,22);
        Item(L"STATIC",L"Delay (s)",0,12,65,65,20); delay=Edit(80,61,65,24,0);
        Item(L"STATIC",L"CH1 probe factor",0,160,65,115,20); probe1=Edit(278,61,65,24,0);
        Item(L"STATIC",L"CH2 probe factor",0,355,65,115,20); probe2=Edit(475,61,65,24,0);
        Item(L"BUTTON",L"Apply local settings",BS_PUSHBUTTON|WS_TABSTOP,12,94,180,26,LocalApply);
        Item(L"BUTTON",L"Read local settings",BS_PUSHBUTTON|WS_TABSTOP,200,94,180,26,LocalRead);
        Label(L"Scope control / status (queries and writes run asynchronously)",135);
        control=Item(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,12,157,540,360,Control);
        for(const auto& f:Tek2221::Controls()) { auto w=Wide(f.label); SendMessageW(control,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(w.c_str())); }
        SendMessageW(control,CB_SETCURSEL,0,0);
        Item(L"BUTTON",L"Read",BS_PUSHBUTTON|WS_TABSTOP,563,157,80,25,Read);
        value=Item(L"COMBOBOX",L"",CBS_DROPDOWN|WS_VSCROLL|WS_TABSTOP,12,192,540,260,Value);
        apply=Item(L"BUTTON",L"Apply value",BS_PUSHBUTTON|WS_TABSTOP,563,192,100,25,Write);
        Label(L"One-shot action (reference Save/Erase overwrites hardware memory)",230);
        actions=Item(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,12,252,540,360,Action);
        for(const auto& a:Tek2221::Actions()) { auto w=Wide(a.second); SendMessageW(actions,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(w.c_str())); }
        SendMessageW(actions,CB_SETCURSEL,0,0);
        Item(L"BUTTON",L"Execute action",BS_PUSHBUTTON|WS_TABSTOP,563,252,120,25,Execute);
        Item(L"STATIC",L"Setup slot 1-10",0,12,292,110,20); slot=Edit(130,288,55,24,Slot); SetText(slot,"1");
        Item(L"STATIC",L"Message row 1-16",0,210,292,130,20); row=Edit(350,288,55,24,Row); SetText(row,"16");
        message=Edit(12,325,650,24,Message); SendMessageW(message,EM_SETLIMITTEXT,40,0);
        activity=Label(L"Ready",364);
        Label(L"Capture status (Ctrl+A/C to copy from any output box)",390); capture=Edit(12,412,750,60,0,true);
        Label(L"Last action result",482); lastAction=Edit(12,504,750,70,0,true);
        Label(L"Last query result",584); lastQuery=Edit(12,606,750,90,0,true);
        ReadLocal(); SelectControl(); Poll();
        tray.cbSize=sizeof(tray); tray.hWnd=window; tray.uID=1;
        tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP; tray.uCallbackMessage=TrayMessage;
        tray.hIcon=LoadIconW(nullptr,MAKEINTRESOURCEW(32516));
        lstrcpynW(tray.szTip,L"Tek2221 controls - double-click to show",128);
        Shell_NotifyIconW(NIM_ADD,&tray); SetTimer(window,1,250,nullptr);
    }
    void ReadLocal()
    {
        SendMessageW(paired,BM_SETCHECK,scope->GetPaired()?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(xy,BM_SETCHECK,scope->GetXY()?BST_CHECKED:BST_UNCHECKED,0);
        SetText(delay,std::to_string(scope->GetCaptureDelay()));
        SetText(probe1,std::to_string(scope->GetChannelAttenuation(0)));
        SetText(probe2,std::to_string(scope->GetChannelAttenuation(1)));
    }
    const Tek2221::ControlSpec& Selected()
    {
        auto n=SendMessageW(control,CB_GETCURSEL,0,0);
        if(n<0 || size_t(n)>=Tek2221::Controls().size()) throw std::runtime_error("Select a control");
        return Tek2221::Controls().at(size_t(n));
    }
    void SelectControl()
    {
        const auto& f=Selected(); SendMessageW(value,CB_RESETCONTENT,0,0);
        for(const auto& c:f.choices) { auto w=Wide(c); SendMessageW(value,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(w.c_str())); }
        auto values=scope->ControlValues(); auto it=values.find(f.key);
        SetText(value,it==values.end()?"":it->second); EnableWindow(apply,!f.command.empty());
    }
    void Queue(const std::string& op,const std::string& arg={})
    {
        if(!scope->QueueControl(op,arg)) throw std::runtime_error("Another scope control operation is pending; wait and retry");
    }
    void Command(int id)
    {
        try
        {
            uiError.clear();
            if(id==Read) { pendingRead=Selected().key; Queue("read:"+pendingRead); }
            else if(id==Write) Queue("write:"+Selected().key,Text(value));
            else if(id==LocalRead) ReadLocal();
            else if(id==LocalApply)
            {
                auto d=Number(Text(delay)),p1=Number(Text(probe1)),p2=Number(Text(probe2));
                if(d<0 || d>60 || p1<=0 || p1>1000 || p2<=0 || p2>1000) throw std::runtime_error("Delay must be 0..60; probe factors >0 and <=1000");
                scope->SetPaired(SendMessageW(paired,BM_GETCHECK,0,0)==BST_CHECKED);
                scope->SetXY(SendMessageW(xy,BM_GETCHECK,0,0)==BST_CHECKED);
                scope->SetCaptureDelay(d); scope->SetChannelAttenuation(0,p1); scope->SetChannelAttenuation(1,p2);
            }
            else if(id==Execute)
            {
                auto n=SendMessageW(actions,CB_GETCURSEL,0,0);
                if(n<0 || size_t(n)>=Tek2221::Actions().size()) throw std::runtime_error("Select an action");
                auto key=Tek2221::Actions()[size_t(n)].first; std::string arg;
                if(key=="setupsave" || key=="setupload") arg=Text(slot);
                if(key=="message") arg=Text(row)+":"+Text(message);
                Queue("action:"+key,arg);
            }
        }
        catch(const std::exception& e) { uiError=std::string("Panel: ")+e.what(); }
        Poll();
    }
    void Poll()
    {
        auto state=scope->StatusSnapshot();
        SetText(activity,!uiError.empty()?uiError:state.operation.empty()?"Ready (close hides this window; tray icon reopens it)":"Working: "+state.operation);
        SetText(capture,state.capture.text); SetText(lastAction,state.action.text); SetText(lastQuery,state.query.text);
        if(!pendingRead.empty() && !scope->ControlBusy())
        {
            if(Selected().key==pendingRead) { auto values=scope->ControlValues(); auto it=values.find(pendingRead); if(it!=values.end()) SetText(value,it->second); }
            pendingRead.clear();
        }
    }
    static LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM w,LPARAM l)
    {
        auto self=reinterpret_cast<Impl*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE)
        {
            self=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); self->window=window;
        }
        if(!self) return DefWindowProcW(window,message,w,l);
        // Never propagate a C++ exception across a Win32 callback boundary.
        try
        {
            if(message==WM_COMMAND)
            {
                if(LOWORD(w)==Control && HIWORD(w)==CBN_SELCHANGE) self->SelectControl();
                else if(HIWORD(w)==BN_CLICKED) self->Command(LOWORD(w));
                return 0;
            }
            if(message==WM_TIMER) { if(self->stopping) DestroyWindow(window); else self->Poll(); return 0; }
            if(message==TrayMessage && (l==WM_LBUTTONDBLCLK || l==WM_LBUTTONUP)) { ShowWindow(window,SW_SHOW); SetForegroundWindow(window); return 0; }
            if(message==WM_CLOSE) { ShowWindow(window,SW_HIDE); return 0; }
            if(message==WM_DESTROY) { KillTimer(window,1); Shell_NotifyIconW(NIM_DELETE,&self->tray); PostQuitMessage(0); return 0; }
        }
        catch(const std::exception& e) { LogError("Tek2221 plugin window: %s\n",e.what()); }
        return DefWindowProcW(window,message,w,l);
    }
    void Run()
    {
        try
        {
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&Procedure),&module);
            WNDCLASSW cls{}; cls.lpfnWndProc=Procedure; cls.hInstance=module;
            cls.lpszClassName=L"Tek2221NativePluginControls"; cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
            cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
            if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register controls window");
            auto title=Wide("Tek2221 controls - "+scope->GetTransportConnectionString());
            window=CreateWindowExW(0,cls.lpszClassName,title.c_str(),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
                CW_USEDEFAULT,CW_USEDEFAULT,795,750,nullptr,nullptr,module,this);
            if(!window) throw std::runtime_error("Cannot create controls window");
            Init(); ShowWindow(window,SW_SHOW);
            MSG msg{};
            while(GetMessageW(&msg,nullptr,0,0)>0) { if(!IsDialogMessageW(window,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
        }
        catch(const std::exception& e) { LogError("Tek2221 plugin controls unavailable: %s\n",e.what()); if(window) DestroyWindow(window); }
    }
};
Tek2221PluginPanel::Tek2221PluginPanel(Tek2221Oscilloscope* scope) : m_impl(new Impl(scope)) {}
Tek2221PluginPanel::~Tek2221PluginPanel()=default;
