// Minimal dynamically loaded 64-bit VISA C ABI. No vendor SDK/import library required.
#ifndef Tek2221VISA_h
#define Tek2221VISA_h
#include "Tek2221Native.h"
#include <memory>

namespace Tek2221
{
class VisaSession : public MessageIO
{
public:
    explicit VisaSession(const std::string& resource = {});
    ~VisaSession() override;
    VisaSession(const VisaSession&) = delete;
    VisaSession& operator=(const VisaSession&) = delete;
    void Write(const std::string& command) override;
    void WriteBytes(const uint8_t* bytes,size_t count);
    std::vector<uint8_t> ReadMessage() override;
    void SetTimeout(unsigned milliseconds);
    std::string LibraryPath() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
#endif
