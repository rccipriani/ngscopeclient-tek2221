// Native 2221 capture and decoding, independent of ngscopeclient and VISA ABI.
#ifndef Tek2221Native_h
#define Tek2221Native_h
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace Tek2221
{
class MessageIO
{
public:
    virtual ~MessageIO() = default;
    virtual void Write(const std::string& command) = 0;
    virtual std::vector<uint8_t> ReadMessage() = 0;
};
struct Record
{
    std::string identity, preamble, coupling, channel, source, format;
    std::vector<uint8_t> raw;
    std::vector<float> samples, minimum, xSamples;
    double xRange = 0;
    std::string xCoupling;
    int64_t interval = 0, phase = 0;
    double range = 0;
    int bits = 0;
};
std::string Query(MessageIO& io, const std::string& command);
std::string Identify(MessageIO& io);
Record Decode(const std::string& preamble, const std::vector<uint8_t>& raw);
Record CaptureChannel(MessageIO& io, unsigned channel, std::function<void(const std::string&)> log = {});
Record CaptureSource(MessageIO& io, unsigned channel, const std::string& source,
    std::function<void(const std::string&)> log = {});
bool Saved(MessageIO& io);
std::vector<Record> CapturePair(MessageIO& io, std::function<void(const std::string&)> log = {});
void ValidatePair(const Record& a, const Record& b);
Record XYComponent(const Record& record, unsigned channel);
Record CaptureCH1(MessageIO& io, std::function<void(const std::string&)> log = {});
}
#endif
