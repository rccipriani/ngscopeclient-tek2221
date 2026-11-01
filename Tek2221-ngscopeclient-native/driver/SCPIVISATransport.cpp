#include "scopehal.h"
#include "SCPIVISATransport.h"
#ifdef TEK2221_PLUGIN
#include "Tek2221Resource.h"
#endif
#include <stdexcept>
#include <algorithm>
#include <cstring>

SCPIVISATransport::SCPIVISATransport(const std::string& resource) : m_resource(resource)
{
    try
    {
        #ifdef TEK2221_PLUGIN
        m_resource=Tek2221::CanonicalGPIBResource(resource);
#endif
        m_session=std::make_unique<Tek2221::VisaSession>(m_resource);
        LogNotice("VISA: 64-bit session opened using %s\n",m_session->LibraryPath().c_str());
    }
    catch(const std::exception& e) { LogError("VISA open failed: %s\n",e.what()); }
}
bool SCPIVISATransport::SetTimeouts(unsigned txUs,unsigned rxUs)
{
    std::lock_guard<std::recursive_mutex> lock(m_netMutex);
    if(!m_session) return false;
    m_session->SetTimeout(std::max(1u,unsigned((uint64_t(std::max(txUs,rxUs))+999)/1000)));
    return true;
}
void SCPIVISATransport::Write(const std::string& command)
{
    std::lock_guard<std::recursive_mutex> lock(m_netMutex);
    if(!m_session) throw std::runtime_error("VISA session is not connected");
    if(m_pendingOffset<m_pending.size()) throw std::runtime_error("Unread VISA message bytes");
    m_session->Write(command);
}
bool SCPIVISATransport::SendCommand(const std::string& command) { Write(command); return true; }
std::vector<uint8_t> SCPIVISATransport::ReadMessage()
{
    std::lock_guard<std::recursive_mutex> lock(m_netMutex);
    if(!m_session) throw std::runtime_error("VISA session is not connected");
    if(m_pendingOffset<m_pending.size()) throw std::runtime_error("Unread VISA message bytes");
    return m_session->ReadMessage();
}
std::string SCPIVISATransport::ReadReply(bool,std::function<void(float)>)
{
    auto data=ReadMessage();
    while(!data.empty() && (data.back()=='\n' || data.back()=='\r')) data.pop_back();
    return std::string(data.begin(),data.end());
}
size_t SCPIVISATransport::ReadRawData(size_t len,unsigned char* buf,std::function<void(float)> progress)
{
    std::lock_guard<std::recursive_mutex> lock(m_netMutex);
    // Respect GPIB EOI: never start another read merely because len is larger than one message.
    if(m_pendingOffset==m_pending.size()) { m_pending=ReadMessage(); m_pendingOffset=0; }
    size_t n=std::min(len,m_pending.size()-m_pendingOffset);
    if(n) std::memcpy(buf,m_pending.data()+m_pendingOffset,n);
    m_pendingOffset+=n;
    if(progress) progress(len?float(n)/len:1);
    return n;
}
void SCPIVISATransport::SendRawData(size_t len,const unsigned char* buf)
{
    std::lock_guard<std::recursive_mutex> lock(m_netMutex);
    if(!m_session) throw std::runtime_error("VISA session is not connected");
    m_session->WriteBytes(buf,len);
}
