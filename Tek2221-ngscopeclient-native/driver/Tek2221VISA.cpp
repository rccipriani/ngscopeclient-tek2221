#include "Tek2221VISA.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <regex>
#include <limits>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Tek2221
{
#ifdef _WIN32
// VISA handles and counts remain 32 bits in the 64-bit API. Attribute state is pointer-sized.
using Status=int32_t;
using Session=uint32_t;
static_assert(sizeof(void*)==8,"Build native VISA support with the 64-bit UCRT64 compiler");
static std::string Hex(Status status)
{
    std::ostringstream s; s<<"0x"<<std::hex<<std::uppercase<<std::setw(8)<<std::setfill('0')<<uint32_t(status); return s.str();
}
static void Check(Status status,const char* operation)
{ if(status<0) throw std::runtime_error(std::string(operation)+" failed: "+Hex(status)); }
struct VisaSession::Impl
{
    HMODULE module=nullptr;
    Session rm=0, session=0;
    std::wstring path;
    Status (WINAPI* openRM)(Session*)=nullptr;
    Status (WINAPI* open)(Session,const char*,uint32_t,uint32_t,Session*)=nullptr;
    Status (WINAPI* close)(Session)=nullptr;
    Status (WINAPI* setAttribute)(Session,uint32_t,uintptr_t)=nullptr;
    Status (WINAPI* write)(Session,const uint8_t*,uint32_t,uint32_t*)=nullptr;
    Status (WINAPI* read)(Session,uint8_t*,uint32_t,uint32_t*)=nullptr;
    template<class T> void Symbol(T& fn,const char* name)
    {
        auto proc=GetProcAddress(module,name);
        if(!proc) throw std::runtime_error(std::string("visa64.dll is missing ")+name);
        static_assert(sizeof(fn)==sizeof(proc),"Windows function-pointer size mismatch");
        std::memcpy(&fn,&proc,sizeof(fn));
    }
    ~Impl()
    {
        if(close && session) close(session);
        if(close && rm) close(rm);
        if(module) FreeLibrary(module);
    }
};
VisaSession::VisaSession(const std::string& resource) : m_impl(new Impl)
{
    auto& i=*m_impl;
    wchar_t system[MAX_PATH]; auto n=GetSystemDirectoryW(system,MAX_PATH);
    if(!n || n>=MAX_PATH) throw std::runtime_error("Cannot locate Windows System32");
    i.path=std::wstring(system,n)+L"\\visa64.dll";
    // Load the standard 64-bit VISA shared dispatcher, never a local/32-bit TekVISA DLL.
    i.module=LoadLibraryExW(i.path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!i.module) throw std::runtime_error("Cannot load System32\\visa64.dll, Windows error "+std::to_string(GetLastError())+". Check the 64-bit NI-VISA runtime installation.");
    i.Symbol(i.openRM,"viOpenDefaultRM"); i.Symbol(i.open,"viOpen"); i.Symbol(i.close,"viClose");
    i.Symbol(i.setAttribute,"viSetAttribute"); i.Symbol(i.write,"viWrite"); i.Symbol(i.read,"viRead");
    Check(i.openRM(&i.rm),"viOpenDefaultRM");
    if(!resource.empty())
    {
        if(!std::regex_match(resource,std::regex("GPIB[0-9]+::[0-9]+(?:::[0-9]+)?::INSTR",std::regex::icase)))
            throw std::runtime_error("Prototype requires a GPIB VISA INSTR resource");
        // Exclusive VISA session prevents another cooperating application changing DATA mid-transaction.
        Check(i.open(i.rm,resource.c_str(),1,10000,&i.session),"viOpen (exclusive; close other instrument sessions)");
        SetTimeout(10000);
        Check(i.setAttribute(i.session,0x3FFF0038u,0),"Disable termchar");
        Check(i.setAttribute(i.session,0x3FFF0016u,1),"Enable GPIB send-END");
    }
}
VisaSession::~VisaSession()=default;
std::string VisaSession::LibraryPath() const
{
    const auto& w=m_impl->path;
    int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);
    std::string result(size_t(n),'\0');
    WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),result.data(),n,nullptr,nullptr);
    return result;
}
void VisaSession::SetTimeout(unsigned ms)
{
    if(!m_impl->session) throw std::runtime_error("No VISA instrument session");
    Check(m_impl->setAttribute(m_impl->session,0x3FFF001Au,ms),"Set VISA timeout");
}
void VisaSession::WriteBytes(const uint8_t* bytes,size_t count)
{
    if(!m_impl->session || count>std::numeric_limits<uint32_t>::max()) throw std::runtime_error("Invalid VISA write");
    uint32_t written=0;
    Check(m_impl->write(m_impl->session,bytes,uint32_t(count),&written),"viWrite");
    if(written!=count) throw std::runtime_error("Short VISA write");
}
void VisaSession::Write(const std::string& command)
{
    auto line=command+"\n";
    WriteBytes(reinterpret_cast<const uint8_t*>(line.data()),line.size());
}
std::vector<uint8_t> VisaSession::ReadMessage()
{
    if(!m_impl->session) throw std::runtime_error("No VISA instrument session");
    std::vector<uint8_t> result;
    for(unsigned chunk=0;chunk<16;chunk++)
    {
        uint8_t buffer[16384]; uint32_t count=0;
        auto status=m_impl->read(m_impl->session,buffer,sizeof(buffer),&count);
        Check(status,"viRead");
        if(count>sizeof(buffer)) throw std::runtime_error("VISA returned an invalid byte count");
        result.insert(result.end(),buffer,buffer+count);
        // VI_SUCCESS_MAX_CNT means continue; VI_SUCCESS denotes GPIB END/EOI.
        if(status==0) return result;
        if(status!=0x3FFF0006) throw std::runtime_error("Unexpected VISA read completion: "+Hex(status));
        if(!count) throw std::runtime_error("VISA read made no progress");
    }
    throw std::runtime_error("VISA response exceeded size limit");
}
#else
struct VisaSession::Impl {};
VisaSession::VisaSession(const std::string&) { throw std::runtime_error("Native VISA prototype requires 64-bit Windows"); }
VisaSession::~VisaSession()=default;
std::string VisaSession::LibraryPath() const { return {}; }
void VisaSession::SetTimeout(unsigned) { throw std::runtime_error("Windows VISA unavailable"); }
void VisaSession::Write(const std::string&) { throw std::runtime_error("Windows VISA unavailable"); }
void VisaSession::WriteBytes(const uint8_t*,size_t) { throw std::runtime_error("Windows VISA unavailable"); }
std::vector<uint8_t> VisaSession::ReadMessage() { throw std::runtime_error("Windows VISA unavailable"); }
#endif
}
