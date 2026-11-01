// CH1/CH2 driver for direct 64-bit VISA, with the v0.1 LAN bridge retained for comparison.
#ifndef Tek2221Oscilloscope_h
#define Tek2221Oscilloscope_h

#include "SCPIOscilloscope.h"
#include <array>
#include <future>
#include "Tek2221Controls.h"
#include "Tek2221Status.h"
#ifdef TEK2221_PLUGIN
class Tek2221PluginPanel;
#endif

class Tek2221Oscilloscope : public SCPIOscilloscope
{
public:
    explicit Tek2221Oscilloscope(SCPITransport* transport);
    ~Tek2221Oscilloscope() override;
    bool IsNative() const;
    // All scope I/O from the custom panel runs off the UI thread.
    bool QueueControl(const std::string& operation,const std::string& value = {});
    std::string ControlStatus();
    Tek2221::PanelStatus StatusSnapshot();
    std::map<std::string,std::string> ControlValues();
    bool ControlBusy();
    void SetPaired(bool paired);
    bool GetPaired();
    void SetXY(bool enabled);
    bool GetXY();
    void SetCaptureDelay(double seconds);
    double GetCaptureDelay();
    std::string IDPing() override;
    unsigned int GetInstrumentTypes() const override { return INST_OSCILLOSCOPE; }
    uint32_t GetInstrumentTypesForChannel(size_t) const override { return INST_OSCILLOSCOPE; }
    void FlushConfigCache() override {}
    bool IsChannelEnabled(size_t) override;
    void EnableChannel(size_t) override;
    void DisableChannel(size_t) override;
    bool CanEnableChannel(size_t i) override { return i < m_channels.size(); }
    OscilloscopeChannel::CouplingType GetChannelCoupling(size_t) override;
    void SetChannelCoupling(size_t, OscilloscopeChannel::CouplingType) override;
    std::vector<OscilloscopeChannel::CouplingType> GetAvailableCouplings(size_t) override;
    double GetChannelAttenuation(size_t) override;
    void SetChannelAttenuation(size_t, double atten) override;
    unsigned int GetChannelBandwidthLimit(size_t) override { return 0; }
    void SetChannelBandwidthLimit(size_t, unsigned int) override;
    OscilloscopeChannel* GetExternalTrigger() override { return nullptr; }
    float GetChannelVoltageRange(size_t, size_t) override;
    void SetChannelVoltageRange(size_t, size_t, float) override;
    float GetChannelOffset(size_t, size_t) override { return 0; }
    void SetChannelOffset(size_t, size_t, float) override;
    TriggerMode PollTrigger() override;
    bool AcquireData() override;
    void Start() override;
    void StartSingleTrigger() override;
    void Stop() override;
    void ForceTrigger() override { StartSingleTrigger(); }
    bool IsTriggerArmed() override;
    void PushTrigger() override;
    void PullTrigger() override {}
    std::vector<std::string> GetTriggerTypes() override { return {}; }
    std::vector<uint64_t> GetSampleRatesNonInterleaved() override;
    std::vector<uint64_t> GetSampleRatesInterleaved() override { return {}; }
    std::vector<uint64_t> GetSampleDepthsNonInterleaved() override;
    std::vector<uint64_t> GetSampleDepthsInterleaved() override { return {}; }
    uint64_t GetSampleRate() override;
    uint64_t GetSampleDepth() override;
    void SetSampleDepth(uint64_t) override;
    void SetSampleRate(uint64_t) override;
    void SetTriggerOffset(int64_t) override;
    int64_t GetTriggerOffset() override;
    bool IsInterleaving() override { return false; }
    bool SetInterleaving(bool) override { return false; }
    std::set<InterleaveConflict> GetInterleaveConflicts() override { return {}; }
    static std::string GetDriverNameInternal() {
#ifdef TEK2221_PLUGIN
        return "tek2221_native";
#else
        return "tek2221";
#endif
    }
    static std::vector<SCPIInstrumentModel> GetDriverSupportedModels();
    static std::shared_ptr<Oscilloscope> CreateInstance(SCPITransport* transport);
    std::string GetDriverName() const override { return GetDriverNameInternal(); }

protected:
    struct Header
    {
        size_t count = 0;
        int64_t interval = 0, phase = 0;
        double range = 0;
        std::string coupling;
        int bits = 0;
    };
    static Header ParseHeader(const std::string& text);
    static void FrontPanel();
    std::array<Header,4> m_headers;
    size_t TimebaseChannel() const { for(size_t i=0;i<m_channels.size();++i) if(m_enabled[i]) return i; return 0; }
    bool AnyEnabled() const { for(size_t i=0;i<m_channels.size();++i) if(m_enabled[i]) return true; return false; }
    void SerializeTek(YAML::Node& node,IDTable& table);
    void LoadTek(int version,const YAML::Node& node,IDTable& table);
    bool m_initialized = false;
    std::array<bool,4> m_enabled = {{true,false,false,false}};
    std::array<double,2> m_attenuation = {{1,1}};
#ifdef TEK2221_PLUGIN
    std::unique_ptr<Tek2221PluginPanel> m_pluginPanel;
#endif
    std::future<void> m_controlTask;
    Tek2221::ControlState m_controlState;
    Tek2221::PanelStatus m_status;
    std::map<std::string,std::string> m_controlValues;
    bool m_controlBusy = false, m_paired = false, m_xy = false, m_plotHold = false;
    double m_captureDelay = 1;
    double m_nextCapture = 0;
    uint64_t m_armGeneration = 0;
};
#endif
