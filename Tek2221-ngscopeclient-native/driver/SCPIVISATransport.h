#ifndef SCPIVISATransport_h
#define SCPIVISATransport_h
#include "SCPITransport.h"
#include "Tek2221VISA.h"
class SCPIVISATransport : public SCPITransport, public Tek2221::MessageIO
{
public:
    explicit SCPIVISATransport(const std::string& resource);
    std::string GetConnectionString() override { return m_resource; }
    static std::string GetTransportName() { return "visa"; }
    static std::vector<TransportEndpoint> EnumTransportEndpoints() { return {}; }
    TRANSPORT_INITPROC(SCPIVISATransport)
    bool SetTimeouts(unsigned txUs,unsigned rxUs) override;
    bool SendCommand(const std::string& command) override;
    std::string ReadReply(bool endOnSemicolon=true,std::function<void(float)> progress=nullptr) override;
    size_t ReadRawData(size_t len,unsigned char* buf,std::function<void(float)> progress=nullptr) override;
    void SendRawData(size_t len,const unsigned char* buf) override;
    bool IsCommandBatchingSupported() override { return false; }
    bool IsConnected() override { return bool(m_session); }
    void Write(const std::string& command) override;
    std::vector<uint8_t> ReadMessage() override;
private:
    std::string m_resource;
    std::unique_ptr<Tek2221::VisaSession> m_session;
    std::vector<uint8_t> m_pending;
    size_t m_pendingOffset=0;
};
#endif
