# Native VISA DLL plugin — v0.6

Use the [repository installation guide](../../README.md) for the supported build
and connection steps. Both UI modes use **one DLL**, `Tek2221Native.dll`:

- **Optional GUI hook installed:** Tek controls render inside Stream Browser.
- **No GUI hook:** the same DLL opens a separate native Windows controls window.

Waveform capture and normal Stream Browser channels work in either mode.
Without the DLL, this project's driver/transport and custom controls are absent.

## Architecture

The upstream Windows plugin loader calls the exported `PluginInit()` entry point.
It registers driver `tek2221_native` and transport `visa`. An empty model list
lets the existing connection dialog enumerate transports without adding a VISA
enum to upstream scopehal. The transport dynamically loads System32/visa64.dll;
NI SDK headers/import libraries and an external bridge are unnecessary.

The driver, transport, native controls window and ImGui sidebar all live in the
DLL. There is one serialized hardware connection. Rendering reads cached state;
capture and explicit control requests perform instrument I/O in the background.
Local settings and setup slots use the host's session serialization.

The external build reads the matching host sources, generated headers and import
libraries. It compiles the host checkout's ImGui sources for the renderer and
shares the host context/allocator through the optional hook. It does not copy
Tek driver files into upstream or change its driver registration/CMake files.
The optional [GUI hook](GUI-HOOK.md) is the only host source integration needed.

The plugin is **not a stable binary SDK**. Use the same source revision, UCRT64
compiler, dependencies and build type on both sides. Release is the plugin
default, matching the documented host build. Matching ImGui version and IO/style
sizes are checked before drawing, but are not a universal C++ ABI guarantee.
Rebuild both sides after relevant dependency changes.

## Scripts

| File | Purpose |
| --- | --- |
| `Build-Plugin.sh HOST_SOURCE [HOST_BUILD]` | Build the external DLL; build directory defaults to HOST_SOURCE/build |
| `Install-Plugin.sh HOST_EXE_DIRECTORY` | Copy DLL into plugins, retaining a .dll.bak of an existing DLL |
| `Run-Stock.sh HOST_EXE_DIRECTORY` | Set the working directory and launch the host |
| `Apply-GUI-Hook.py HOST_SOURCE` | Optionally install the generic sidebar hook; host rebuild required |

The Windows loader finds plugins relative to the EXE but uses a relative path
when loading them. Run-Stock.sh accommodates that behavior without changing the
loader. Launch from UCRT64 for dependency DLL resolution. Installation does not
replace host DLLs or register VISA/COM components.

The plugin canonicalizes collapsed valid GPIB separators from the recent menu,
e.g. `GPIB0:5:INSTR` to `GPIB0::5::INSTR`, without patching the host menu parser.

## Separate controls window

The native window offers local capture/probe settings, explicit scope Read/Apply,
action selection, and separate capture/action/query text outputs. Use **Apply
local settings** to commit local fields and **Read local settings** to reload
cached preferences. It uses documented command abbreviations for enum values;
the integrated sidebar uses friendlier labels. Text outputs support normal copy
operations. Closing hides the window; its tray icon reopens it. Removing the
instrument closes the controls thread before releasing the driver.

See [troubleshooting](../../docs/TROUBLESHOOTING.md) and
[validation](../VALIDATION.md). The original built-in driver/bridge remain
historical alternatives, not dependencies of this plugin.
