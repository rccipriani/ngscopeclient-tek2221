#ifndef Tek2221PluginPanel_h
#define Tek2221PluginPanel_h
#include <memory>
class Tek2221Oscilloscope;
// Native Win32 window, independent of ngscopeclient's ImGui implementation.
class Tek2221PluginPanel {
public:
    explicit Tek2221PluginPanel(Tek2221Oscilloscope* scope);
    ~Tek2221PluginPanel();
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
#endif
