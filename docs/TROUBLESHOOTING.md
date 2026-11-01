# Troubleshooting and diagnostics

Start with the [installation guide](../README.md). Use a matching host/plugin
build in MSYS2 UCRT64; close competing scope applications.

## Plugin or controls missing

- Confirm `Tek2221Native.dll` is in the host executable's `plugins` directory.
- Launch with `plugin/Run-Stock.sh` from UCRT64. The Windows loader needs the
  correct working directory and the runtime dependencies on PATH.
- Look for `Tek2221 native VISA DLL plugin loaded` in the startup log.
- Select driver `tek2221_native`, not the historical built-in `tek2221`.
- Without the optional GUI hook, no custom sidebar is expected. Use the separate
  **Tek2221 controls** window; its tray icon reopens it after closing hides it.
- With the hook, rebuild the host after applying it and expand the instrument
  and **Tek 2221 controls (v0.4)** in Stream Browser. That panel label is retained
  in v0.6. No separate controls window is expected.
- If the log reports incompatible host ImGui, rebuild the DLL from the exact
  host sources with the matching compiler, dependencies and build type. The
  hook suppresses the separate window when present; a rejected sidebar renderer
  does not automatically open that fallback window.

## Vulkan assertion on first capture

`getVkHeaderVersion() == VK_HEADER_VERSION` can indicate a C++ ABI mismatch,
not an instrument problem. Release/Debug changes the Vulkan dispatcher layout.
Use matching host/plugin build types (Release in the documented installation),
Vulkan headers and `VK_ENABLE_BETA_EXTENSIONS=1`. v0.6 sets the plugin Release
 default and beta-extension definition. After dependency upgrades, rebuild both
host and plugin. Do not bypass the assertion to conceal a layout mismatch.

## NI adapter or VISA errors

First verify the adapter and instrument in NI MAX. The tested setup used an
NI-compatible GPIB-USB-HS clone, NI-488.2 and NI-VISA installed through NI Package
Manager. Clone compatibility varies. TekVISA is unnecessary; an old 32-bit
TekVISA installation caused access violations during development. Repairing the
NI installation and rebooting resolved that problem. Do not copy arbitrary VISA
DLLs into the app directory.

Use the actual resource address, e.g. `GPIB0::5::INSTR`. The driver normalizes
collapsed GPIB colons from older recent-menu entries. It opens the resource
exclusively: close NI MAX interactive sessions, WaveStar and bridge processes.
After an unplug/timeout, fix the connection and remove/re-add the instrument.
The driver does not automatically reset the bus or clear/reconnect the scope.

## Missing, flat or incorrectly scaled waveforms

- Check STORE and physical channel selection; BOTH + ALT is needed for dual
  channel use. A flat input should produce a flat capture.
- Set the correct physical probe attenuation in software. A 10× probe's BNC
  voltage is one tenth of the tip voltage; the scope cannot sense the probe switch.
- Add the channel's Signal / Max stream and use the time ruler's Autofit.
- Paired capture requires manually pressing SAVE and keeping controls unchanged.
- An empty or incompatible REF4 is an error; save a reference before reading it.
- Use Capture status to distinguish new data from the previous retained trace.

PC cursors do not move the scope cursors. The scope's horizontal-position knob
moves the CRT window and need not change the exported record's time origin.
Physical analog settings cannot be overridden by generic PC widgets.

## MSYS2 under Program Files

For compiler temporary-file permission errors, choose a writable temporary path:

```bash
mkdir -p "$HOME/tmp/tek2221-build"
export TMPDIR="$HOME/tmp/tek2221-build"
export TMP="$TMPDIR"
export TEMP="$TMPDIR"
```

Retry the failed build. The main guide includes windres preprocessor arguments
for paths with spaces and OpenMP linker flags used in the tested setup.

## Standalone VISA checks

From `Tek2221-ngscopeclient-native`, close ngscopeclient and competing sessions:

```bash
bash Build-Native-Check.sh
./Tek2221VisaCheck.exe --load-only
./Tek2221VisaCheck.exe --id 'GPIB0::5::INSTR'
./Tek2221VisaCheck.exe --capture 'GPIB0::5::INSTR' native-results
./Tek2221VisaCheck.exe --capture-ch2 'GPIB0::5::INSTR' native-results-ch2
# First select BOTH + ALT and press SAVE:
./Tek2221VisaCheck.exe --pair 'GPIB0::5::INSTR' paired-results
# Only if compatible REF4 data exists:
./Tek2221VisaCheck.exe --ref4 'GPIB0::5::INSTR' ref4-results
./Tek2221VisaCheck.exe --ref4-ch2 'GPIB0::5::INSTR' ref4-results-ch2
```

Diagnostic output includes preambles, checksum-validated raw responses and
calibrated BNC-voltage CSVs before probe attenuation. Paired outputs have CH1/CH2
subdirectories. The GUI does not automatically write those capture folders; use
ngscopeclient's session/export facilities to retain traces.

Offline decoder/control/status/resource tests:

```bash
bash Run-Offline-Tests.sh
```

For a bug report, include the host revision, compiler/dependency versions,
sidebar or separate-window mode, resource address, physical scope modes, actions
immediately before failure, and terminal output. When possible attach a diagnostic
preamble/raw capture. See the [acceptance checklist](../Tek2221-ngscopeclient-native/VALIDATION.md).

## Update or remove

For plugin source updates, close ngscopeclient, pull this repository, then rerun
Build-Plugin.sh and Install-Plugin.sh against the matching host. A host/API,
compiler or dependency update may require rebuilding both. Do not install an
arbitrary DLL built against a different host.

To remove the plugin, close ngscopeclient and remove or rename
`plugins/Tek2221Native.dll`. The `.dll.bak` file retains a previous installation.
The generic hook may remain; without a renderer DLL it has no Tek panel to draw.

To remove only the sidebar hook, follow
[GUI-HOOK.md](../Tek2221-ngscopeclient-native/plugin/GUI-HOOK.md) and rebuild the
host. Keep the DLL installed for the separate-window mode. Existing unrelated
source edits should be preserved; do not blindly restore an old backup over them.
