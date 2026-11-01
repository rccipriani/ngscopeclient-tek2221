#include "scopehal.h"
#include "Tek2221Oscilloscope.h"
#include "SCPIVISATransport.h"
#ifdef TEK2221_PLUGIN
#include "Tek2221PluginPanel.h"
#include <windows.h>
#endif
#include <sstream>
#include <cstring>
#include <cmath>
#include <limits>

using namespace std;
namespace {
class Tek2221Channel : public OscilloscopeChannel {
public:
    Tek2221Channel(Oscilloscope* scope,const string& name,const string& color,size_t index)
        : OscilloscopeChannel(scope,name,color,Unit(Unit::UNIT_FS),index)
    {
        AddStream(Unit(Unit::UNIT_VOLTS),"Signal / Max",Stream::STREAM_TYPE_ANALOG);
        AddStream(Unit(Unit::UNIT_VOLTS),"Min",Stream::STREAM_TYPE_ANALOG);
    }
};
}

Tek2221Oscilloscope::Tek2221Oscilloscope(SCPITransport* transport)
    : SCPIDevice(transport, transport->GetName() != "visa")
    , SCPIInstrument(transport, transport->GetName() != "visa"), SCPIOscilloscope()
{
    m_triggerArmed = false;
    m_triggerOneShot = false;
    try
    {
        m_transport->SetTimeouts(10000000, 10000000);
        if(auto native = dynamic_cast<SCPIVISATransport*>(transport))
        {
            lock_guard<recursive_mutex> transaction(m_transport->GetMutex());
            auto r = Tek2221::CaptureCH1(*native, [](const string& message) { LogNotice("Tek2221: %s\n",message.c_str()); });
            if(r.format=="XY") r=Tek2221::XYComponent(r,1);
            m_headers[0] = {r.samples.size(), r.interval, r.phase, r.range, r.coupling, r.bits};
            m_vendor = "TEKTRONIX";
            m_model = r.identity.find("2221A") != string::npos ? "2221A" : "2221";
            m_serial = "GPIB-" + transport->GetConnectionString(); // 2221 ID has no serial number
            m_fwVersion = r.identity;
        }
        else
        {
            m_transport->SetTimeouts(30000000, 30000000);
            if(m_model != "2221-BRIDGE") throw runtime_error("Expected direct VISA or the Tek2221 LAN bridge");
            m_headers[0] = ParseHeader(m_transport->SendCommandImmediateWithReply("META?"));
        }
    }
    catch(const exception& e)
    {
        LogError("Tek2221 connection failed: %s\n", e.what());
        return;
    }
    // CH2 is exposed only on the native transport. Start with CH1 enabled.
    size_t count = dynamic_cast<SCPIVISATransport*>(transport) ? 4 : 1;
    for(size_t i=1;i<count;++i) m_headers[i] = m_headers[0]; // provisional until the first CH2 capture
    for(size_t i=0; i<count; ++i)
    {
        auto ch = new Tek2221Channel(this, (i>=2?"REF4 ":"") + string("CH") + to_string(i%2+1),
            i>=2?(i%2?"#00aaaa":"#aaaa00"):(i%2?"#00ffff":"#ffff00"), i);
        m_channels.push_back(ch);
        ch->SetDefaultDisplayName();
    }
    m_serializers.push_back(sigc::mem_fun(*this,&Tek2221Oscilloscope::SerializeTek));
    m_loaders.push_back(sigc::mem_fun(*this,&Tek2221Oscilloscope::LoadTek));
    m_initialized = true;
#ifdef TEK2221_PLUGIN
    if(IsNative() && !GetProcAddress(GetModuleHandleW(nullptr),"NgscopeclientStreamPanelHookVersion"))
    {
        try { m_pluginPanel=make_unique<Tek2221PluginPanel>(this); }
        catch(const exception& e) { LogError("Tek2221 controls window unavailable: %s\n",e.what()); }
    }
#endif
}

shared_ptr<Oscilloscope> Tek2221Oscilloscope::CreateInstance(SCPITransport* transport)
{
    auto scope = make_shared<Tek2221Oscilloscope>(transport);
    if(!scope->m_initialized)
    {
        // The GUI's failed-factory path deletes the caller-owned transport.
        // Prevent SCPIDevice's destructor from deleting it a second time.
        scope->m_transport = nullptr;
        return nullptr;
    }
    return scope;
}

vector<SCPIInstrumentModel> Tek2221Oscilloscope::GetDriverSupportedModels()
{
#ifdef TEK2221_PLUGIN
    // Empty model list activates the stock GUI's registered-transport dropdown.
    // No additional SCPITransportType enum value is needed.
    return {};
#else
    return {
#ifdef _WIN32
        {"Tektronix 2221 (native 64-bit VISA v0.4, full storage)", {{SCPITransportType::TRANSPORT_VISA, "GPIB0::5::INSTR"}}},
#endif
        {"Tektronix 2221 (legacy LAN bridge)", {{SCPITransportType::TRANSPORT_LAN, "127.0.0.1:22210"}}}
    };
#endif
}

string Tek2221Oscilloscope::IDPing()
{
    { lock_guard<recursive_mutex> lock(m_cacheMutex); if(m_plotHold) return m_fwVersion; }
    if(auto native = dynamic_cast<SCPIVISATransport*>(m_transport))
    {
        lock_guard<recursive_mutex> transaction(m_transport->GetMutex());
        { lock_guard<recursive_mutex> lock(m_cacheMutex); if(m_plotHold) return m_fwVersion; }
        try { return Tek2221::Identify(*native); }
        catch(const exception& e)
        {
            LogError("Tek2221 identity check failed: %s\n",e.what());
            return {};
        }
    }
    return SCPIOscilloscope::IDPing();
}

Tek2221Oscilloscope::Header Tek2221Oscilloscope::ParseHeader(const string& text)
{
    Header h;
    string magic, extra;
    int version = 0;
    istringstream s(text);
    s.imbue(locale::classic());
    if(!(s >> magic >> version >> h.count >> h.interval >> h.phase >> h.range >> h.coupling >> h.bits)
        || (s >> extra) || magic != "T2221" || version != 1 || !h.count || h.count > 4096
        || h.interval <= 0 || h.interval > 1000000000000000000LL || !isfinite(h.range) || h.range <= 0
        || (h.bits != 8 && h.bits != 16) || (h.coupling != "AC" && h.coupling != "DC" && h.coupling != "GND"))
        throw runtime_error("Invalid/failed Tek2221 bridge reply: " + text);
    return h;
}

void Tek2221Oscilloscope::FrontPanel()
{ LogWarning("Tek2221: this setting requires the scope front panel; no hardware command sent\n"); }

bool Tek2221Oscilloscope::IsChannelEnabled(size_t i)
{ lock_guard<recursive_mutex> lock(m_cacheMutex); return i < m_channels.size() && m_enabled[i]; }
void Tek2221Oscilloscope::EnableChannel(size_t i)
{ lock_guard<recursive_mutex> lock(m_cacheMutex); if(i < m_channels.size() && !m_enabled[i]) { m_enabled[i] = true; ++m_armGeneration; } }
void Tek2221Oscilloscope::DisableChannel(size_t i)
{ lock_guard<recursive_mutex> lock(m_cacheMutex); if(i < m_channels.size() && m_enabled[i]) { m_enabled[i] = false; ++m_armGeneration; } }
OscilloscopeChannel::CouplingType Tek2221Oscilloscope::GetChannelCoupling(size_t i)
{
    lock_guard<recursive_mutex> lock(m_cacheMutex);
    if(m_headers.at(i).coupling == "AC") return OscilloscopeChannel::COUPLE_AC_1M;
    if(m_headers.at(i).coupling == "GND") return OscilloscopeChannel::COUPLE_GND;
    return OscilloscopeChannel::COUPLE_DC_1M;
}
vector<OscilloscopeChannel::CouplingType> Tek2221Oscilloscope::GetAvailableCouplings(size_t i)
{ return {GetChannelCoupling(i)}; }
void Tek2221Oscilloscope::SetChannelCoupling(size_t, OscilloscopeChannel::CouplingType) { FrontPanel(); }
double Tek2221Oscilloscope::GetChannelAttenuation(size_t i)
{ lock_guard<recursive_mutex> lock(m_cacheMutex); return m_attenuation.at(i%2); }
void Tek2221Oscilloscope::SetChannelAttenuation(size_t i, double atten)
{
    lock_guard<recursive_mutex> lock(m_cacheMutex);
    if(i < m_channels.size() && isfinite(atten) && atten > 0 && atten <= 1000 && m_attenuation[i%2] != atten)
    { m_attenuation[i%2] = atten; ++m_armGeneration; }
}
float Tek2221Oscilloscope::GetChannelVoltageRange(size_t i, size_t)
{ lock_guard<recursive_mutex> lock(m_cacheMutex); return m_headers.at(i).range * m_attenuation.at(i%2); }
void Tek2221Oscilloscope::SetChannelVoltageRange(size_t, size_t, float) { FrontPanel(); }
void Tek2221Oscilloscope::SetChannelOffset(size_t, size_t, float) { FrontPanel(); }
void Tek2221Oscilloscope::SetChannelBandwidthLimit(size_t, unsigned int) { FrontPanel(); }
void Tek2221Oscilloscope::SetSampleDepth(uint64_t) { FrontPanel(); }
void Tek2221Oscilloscope::SetSampleRate(uint64_t) { FrontPanel(); }
void Tek2221Oscilloscope::SetTriggerOffset(int64_t) { FrontPanel(); }
void Tek2221Oscilloscope::PushTrigger() { FrontPanel(); }
uint64_t Tek2221Oscilloscope::GetSampleRate()
{ lock_guard<recursive_mutex> lock(m_cacheMutex); return llround(double(FS_PER_SECOND) / m_headers[TimebaseChannel()].interval); }
uint64_t Tek2221Oscilloscope::GetSampleDepth()
{ lock_guard<recursive_mutex> lock(m_cacheMutex); return m_headers[TimebaseChannel()].count; }
int64_t Tek2221Oscilloscope::GetTriggerOffset()
{ lock_guard<recursive_mutex> lock(m_cacheMutex); return -m_headers[TimebaseChannel()].phase; }
vector<uint64_t> Tek2221Oscilloscope::GetSampleRatesNonInterleaved() { return {GetSampleRate()}; }
vector<uint64_t> Tek2221Oscilloscope::GetSampleDepthsNonInterleaved() { return {GetSampleDepth()}; }

void Tek2221Oscilloscope::Start()
{
    lock_guard<recursive_mutex> lock(m_cacheMutex);
    ++m_armGeneration; m_triggerArmed = true; m_triggerOneShot = false; m_nextCapture = 0;
}
void Tek2221Oscilloscope::StartSingleTrigger()
{
    lock_guard<recursive_mutex> lock(m_cacheMutex);
    ++m_armGeneration; m_triggerArmed = true; m_triggerOneShot = true; m_nextCapture = 0;
}
void Tek2221Oscilloscope::Stop()
{
    lock_guard<recursive_mutex> lock(m_cacheMutex);
    ++m_armGeneration; m_triggerArmed = false;
}
bool Tek2221Oscilloscope::IsTriggerArmed()
{ lock_guard<recursive_mutex> lock(m_cacheMutex); return m_triggerArmed; }
Oscilloscope::TriggerMode Tek2221Oscilloscope::PollTrigger()
{
    lock_guard<recursive_mutex> lock(m_cacheMutex);
    if(!m_triggerArmed) return TRIGGER_MODE_STOP;
    if(!m_plotHold && !m_controlBusy && AnyEnabled() && GetTime() >= m_nextCapture) return TRIGGER_MODE_TRIGGERED;
    return TRIGGER_MODE_RUN;
}

bool Tek2221Oscilloscope::AcquireData()
{
    uint64_t generation;
    array<bool,4> enabled;
    bool paired, xy;
    array<double,2> attenuation;
    {
        lock_guard<recursive_mutex> lock(m_cacheMutex);
        enabled = m_enabled;
        if(!m_triggerArmed || m_plotHold || m_controlBusy || !AnyEnabled()) return false;
        paired=m_paired; xy=m_xy;
        generation = m_armGeneration;
        attenuation = m_attenuation;
    }
    try
    {
        lock_guard<recursive_mutex> transaction(m_transport->GetMutex());
        { lock_guard<recursive_mutex> lock(m_cacheMutex); if(generation!=m_armGeneration || !m_triggerArmed || m_plotHold || m_controlBusy) return false; }
        m_transport->FlushCommandQueue();
        array<Header,4> headers;
        array<array<unique_ptr<UniformAnalogWaveform>,2>,4> captures;
        array<Tek2221::Record,4> records;
        array<bool,4> have = {{false,false,false,false}};
        auto native=dynamic_cast<SCPIVISATransport*>(m_transport);
        bool held=paired && (enabled[0] || enabled[1]);
        auto checkSave=[&]() {
            if(held && !Tek2221::Saved(*native)) throw runtime_error("Paired capture requires BOTH + ALT and SAVE. Leave controls unchanged during transfer.");
        };
        if(held && !native) throw runtime_error("Paired capture requires native VISA");
        if(held)
        {
            auto pair=Tek2221::CapturePair(*native,[](const string& message) { LogNotice("Tek2221: %s\n",message.c_str()); });
            if(xy && pair[0].format!="XY") throw runtime_error("Native XY selected: press the physical X-Y switch in STORE mode");
            records[0]=std::move(pair[0]); records[1]=std::move(pair[1]); have[0]=have[1]=true;
        }
        for(size_t channel=0; channel<m_channels.size(); ++channel)
        {
            // Paired mode reads both acquisition records even if only one is displayed.
            if(!enabled[channel] && !(held && channel<2)) continue;
            Header h;
            vector<float> samples, minimum;
            if(native)
            {
                if(!have[channel])
                {
                    if(channel<2) checkSave();
                    auto r=Tek2221::CaptureSource(*native,xy?1:unsigned(channel%2+1),channel<2?"ACQ":"REF4",
                        [](const string& message) { LogNotice("Tek2221: %s\n",message.c_str()); });
                    if(xy && r.format!="XY") throw runtime_error("Native XY selected: press the physical X-Y switch in STORE mode");
                    if(r.format=="XY")
                    {
                        auto base=channel<2?0:2;
                        for(unsigned axis=0;axis<2;++axis) { records[base+axis]=Tek2221::XYComponent(r,axis+1); have[base+axis]=true; }
                    }
                    else { records[channel]=std::move(r); have[channel]=true; }
                }
                const auto& r=records[channel];
                h={r.samples.size(),r.interval,r.phase,r.range,r.coupling,r.bits};
                samples=r.samples; minimum=r.minimum;
            }
            else
            {
                h = ParseHeader(m_transport->SendCommandImmediateWithReply("CAPTURE?"));
                vector<unsigned char> bytes(h.count * 4);
                if(m_transport->ReadRawData(bytes.size(), bytes.data()) != bytes.size())
                    throw runtime_error("Truncated Tek2221 sample payload; reconnect the bridge");
                samples.resize(h.count);
                static_assert(sizeof(float) == 4 && numeric_limits<float>::is_iec559, "IEEE754 float32 required");
                for(size_t i=0; i<h.count; i++)
                {
                    uint32_t u = uint32_t(bytes[4*i]) | (uint32_t(bytes[4*i+1]) << 8)
                        | (uint32_t(bytes[4*i+2]) << 16) | (uint32_t(bytes[4*i+3]) << 24);
                    memcpy(&samples[i], &u, 4);
                    if(!isfinite(samples[i])) throw runtime_error("Nonfinite Tek2221 sample");
                }
            }
            headers[channel]=h;
            if(!enabled[channel]) continue;
            // In plain Y/XY records both bounds are equal; ENV preserves both bounds without downsampling.
            for(size_t stream=0;stream<2;++stream)
            {
                const auto& values=stream==1 && !minimum.empty()?minimum:samples;
                auto cap=make_unique<UniformAnalogWaveform>();
                cap->m_timescale=h.interval; cap->m_triggerPhase=h.phase;
                cap->Resize(h.count); cap->PrepareForCpuAccess();
                for(size_t i=0;i<h.count;++i) cap->m_samples[i]=values[i]*attenuation[channel%2];
                cap->MarkSamplesModifiedFromCpu(); captures[channel][stream]=std::move(cap);
            }
        }
        checkSave();
        if(held) Tek2221::ValidatePair(records[0],records[1]);
        lock_guard<recursive_mutex> lock(m_cacheMutex);
        if(generation != m_armGeneration || !m_triggerArmed) return false;
        // Publish a complete batch only. These are sequential transfers, not simultaneous acquisitions.
        double now = GetTime();
        SequenceSet set;
        for(size_t channel=0; channel<m_channels.size(); ++channel)
        {
            if(!captures[channel][0]) continue;
            m_headers[channel] = headers[channel];
            for(size_t stream=0;stream<2;++stream)
            {
                captures[channel][stream]->m_startTimestamp=floor(now);
                captures[channel][stream]->m_startFemtoseconds=(now-floor(now))*FS_PER_SECOND;
                set[StreamDescriptor(GetOscilloscopeChannel(channel),stream)]=captures[channel][stream].release();
            }
        }
        {
            lock_guard<mutex> pending(m_pendingWaveformsMutex);
            m_pendingWaveforms.push_back(set);
        }
        m_status.Capture(held ? "Paired capture OK: SAVE checked before/between/after; timing matched. ALT sampling is sequential." : "Capture OK (live records are not a synchronized pair).",true);
        m_nextCapture = GetTime() + m_captureDelay;
        if(m_triggerOneShot) m_triggerArmed = false;
        return true;
    }
    catch(const exception& e)
    {
        LogError("Tek2221 acquisition failed: %s\n", e.what());
        { lock_guard<recursive_mutex> lock(m_cacheMutex); m_status.Capture(string("Capture error: ")+e.what(),false); }
        Stop();
        return false;
    }
}

Tek2221Oscilloscope::~Tek2221Oscilloscope()
{
#ifdef TEK2221_PLUGIN
    m_pluginPanel.reset(); // join native controls thread before releasing driver members
#endif
    // Complete outstanding UI I/O before base classes release the VISA session.
    if(m_controlTask.valid()) m_controlTask.wait();
}
bool Tek2221Oscilloscope::IsNative() const { return dynamic_cast<SCPIVISATransport*>(m_transport)!=nullptr; }
bool Tek2221Oscilloscope::ControlBusy() { lock_guard<recursive_mutex> lock(m_cacheMutex); return m_controlBusy; }
string Tek2221Oscilloscope::ControlStatus() { lock_guard<recursive_mutex> lock(m_cacheMutex); return m_status.capture.text; }
Tek2221::PanelStatus Tek2221Oscilloscope::StatusSnapshot() { lock_guard<recursive_mutex> lock(m_cacheMutex); return m_status; }
map<string,string> Tek2221Oscilloscope::ControlValues() { lock_guard<recursive_mutex> lock(m_cacheMutex); return m_controlValues; }
bool Tek2221Oscilloscope::GetPaired() { lock_guard<recursive_mutex> lock(m_cacheMutex); return m_paired; }
void Tek2221Oscilloscope::SetPaired(bool value) { lock_guard<recursive_mutex> lock(m_cacheMutex); if(m_paired!=value) { m_paired=value; ++m_armGeneration; } }
bool Tek2221Oscilloscope::GetXY() { lock_guard<recursive_mutex> lock(m_cacheMutex); return m_xy; }
void Tek2221Oscilloscope::SetXY(bool value) { lock_guard<recursive_mutex> lock(m_cacheMutex); if(m_xy!=value) { m_xy=value; ++m_armGeneration; } }
double Tek2221Oscilloscope::GetCaptureDelay() { lock_guard<recursive_mutex> lock(m_cacheMutex); return m_captureDelay; }
void Tek2221Oscilloscope::SetCaptureDelay(double value)
{
    if(!isfinite(value) || value<0 || value>60) return;
    lock_guard<recursive_mutex> lock(m_cacheMutex); m_captureDelay=value; m_nextCapture=0;
}
bool Tek2221Oscilloscope::QueueControl(const string& operation,const string& value)
{
    lock_guard<recursive_mutex> lock(m_cacheMutex);
    if(!IsNative() || m_controlBusy) return false;
    // Never destroy a not-yet-completed future while holding the cache mutex.
    if(m_controlTask.valid() && m_controlTask.wait_for(chrono::seconds(0))!=future_status::ready) return false;
    bool write=operation.compare(0,6,"write:")==0;
    bool action=operation.compare(0,7,"action:")==0;
    if(write || action) ++m_armGeneration;
    if(operation=="action:plotstart" || (operation=="write:AUT" && value=="ON")) m_triggerArmed=false;
    auto state=m_controlState;
    string label=operation;
    const auto key=operation.substr(operation.find(':')+1);
    if(action)
    { for(const auto& item:Tek2221::Actions()) if(item.first==key) { label=item.second; break; } }
    else
    { for(const auto& item:Tek2221::Controls()) if(item.key==key) { label=item.label; break; } }
    if(write) label += " = "+value;
    m_controlBusy=true; m_status.Begin(label);
    m_controlTask=async(launch::async,[this,operation,value,state,label]() mutable {
        lock_guard<recursive_mutex> transaction(m_transport->GetMutex());
        string result; bool ok=false;
        try
        {
            auto& io=*dynamic_cast<SCPIVISATransport*>(m_transport);
            m_transport->FlushCommandQueue();
            if(state.plotHold && operation!="action:plotabort") throw runtime_error("Legacy plot active: Abort plot before other commands");
            if(operation.compare(0,5,"read:")==0) result=Tek2221::ReadControl(io,operation.substr(5));
            else if(operation.compare(0,6,"write:")==0)
            {
                // Capture decoders and safe DATA restoration require verbose replies.
                if(operation=="write:LONG" && value=="OFF") throw runtime_error("Native capture requires LONG ON (verbose replies)");
                if(operation=="write:AUT" && value=="ON") state.plotHold=true;
                result=Tek2221::WriteControl(io,operation.substr(6),value);
            }
            else if(operation.compare(0,7,"action:")==0) result=state.Execute(io,operation.substr(7),value);
            else throw runtime_error("Unknown panel operation");
            ok=true;
        }
        catch(const exception& e) { result=string("Error: ")+e.what(); LogError("Tek2221 control: %s\n",e.what()); }
        lock_guard<recursive_mutex> done(m_cacheMutex);
        if(ok && (operation.compare(0,5,"read:")==0 || operation.compare(0,6,"write:")==0))
            m_controlValues[operation.substr(operation.find(':')+1)]=result;
        m_controlState=std::move(state); m_plotHold=m_controlState.plotHold;
        m_status.Complete(operation.compare(0,5,"read:")==0,label,result,ok); m_controlBusy=false;
    });
    return true;
}
void Tek2221Oscilloscope::SerializeTek(YAML::Node& node,IDTable&)
{
    lock_guard<recursive_mutex> lock(m_cacheMutex);
    node["tek2221"]["xy"]=m_xy; node["tek2221"]["paired"]=m_paired; node["tek2221"]["capture_delay_seconds"]=m_captureDelay;
    for(size_t i=0;i<10;++i) node["tek2221"]["setup_slots"][i]=m_controlState.slots[i];
}
void Tek2221Oscilloscope::LoadTek(int,const YAML::Node& node,IDTable&)
{
    if(!node["tek2221"]) return;
    const auto n=node["tek2221"];
    if(n["xy"]) SetXY(n["xy"].as<bool>());
    if(n["paired"]) SetPaired(n["paired"].as<bool>());
    if(n["capture_delay_seconds"]) SetCaptureDelay(n["capture_delay_seconds"].as<double>());
    lock_guard<recursive_mutex> lock(m_cacheMutex);
    if(n["setup_slots"] && n["setup_slots"].IsSequence())
        for(size_t i=0;i<min(size_t(10),n["setup_slots"].size());++i)
        {
            auto text=n["setup_slots"][i].as<string>();
            if(!text.empty()) Tek2221::ParseSetup(text);
            m_controlState.slots[i]=text;
        }
}
