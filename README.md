# Tektronix 2221 support for ngscopeclient

**Version 0.6 — native 64-bit VISA driver with optional integrated controls.**

Connect a Tektronix 2221 digital storage oscilloscope to
[ngscopeclient](https://www.ngscopeclient.org/) over GPIB. Capture acquisition and
reference waveforms, use ngscopeclient's plotting and analysis tools, and operate
the scope's supported digital settings from a PC.

The plugin talks directly to the installed 64-bit VISA runtime. It does not need
WaveStar, TekVISA, a network bridge, or a separate capture service.

```text
ngscopeclient + Tek2221Native.dll → 64-bit VISA → GPIB adapter → Tektronix 2221
```

## What it supports

- CH1 and CH2 acquisition memory, plus REF4 CH1 and REF4 CH2.
- Unsigned 8-bit and 16-bit waveforms, calibrated voltage/time axes, and manual
  per-channel probe attenuation.
- Envelope maximum/minimum streams and native XY record decoding.
- Repeated live transfers and SAVE-guarded paired reads for comparing channels.
- Digital acquisition settings, scope cursors, reference-memory actions,
  local setup save/restore slots, CRT messages and interface settings.
- Capture, action and query status, plus session persistence of local settings.

See the [operating guide](docs/USAGE.md) for details and limitations. This is a
community project, not a Tektronix or NI product. It has been tested on a 2221
with V15 ROMs and the GPIB option; compatibility with a 2221A or other ROM versions
has not been established here.

## Is the sidebar optional?

**Yes. The host GUI hook is optional; the driver DLL is required.** There is no
separate sidebar DLL to install. `Tek2221Native.dll` contains the driver, VISA
transport, integrated sidebar renderer and fallback Windows controls window.

| Installation | Waveform capture | Tek-specific controls | Upstream source changes |
| --- | --- | --- | --- |
| DLL + GUI hook (recommended) | Yes | Inside Stream Browser | One include, one render call and one new header |
| DLL only | Yes | Separate native Windows window | None |
| GUI hook only, without DLL | No 2221 driver from this project | None | Hook has no Tek renderer to call |

In DLL-only mode, closing the controls window hides it; its tray icon reopens it.
With the hook present, the separate window is suppressed and controls appear
under the scope in Stream Browser. Both modes use the same instrument connection.

## Requirements

- Windows 11 x64 (the tested platform), with a Vulkan-capable GPU supported by
  ngscopeclient.
- A working Tektronix 2221 with the GPIB interface option installed. V15 ROMs were
  used during testing.
- A compatible GPIB adapter. Testing used an **NI-compatible GPIB-USB-HS clone**;
  this does not establish that every clone or adapter works.
- **NI Package Manager**, **NI-488.2** for the adapter, and **NI-VISA with 64-bit
  runtime support**. The tested NI installation was 2026 Q3. Confirm that NI MAX
  sees both the adapter and the scope before testing the plugin. Close NI MAX
  interactive instrument sessions before connecting from ngscopeclient.
- **MSYS2 UCRT64**, Git, Python, CMake, Ninja, the UCRT64 C++ toolchain and the
  dependencies needed to build ngscopeclient. Follow the upstream
  [Windows build prerequisites](https://www.ngscopeclient.org/manual/GettingStarted.html).

TekVISA is not required. An old 32-bit TekVISA installation caused access
violations during earlier work; repairing the NI stack and rebooting resolved
that problem. Do not manually copy VISA DLLs into the application directory.
The plugin loads `visa64.dll` from the Windows system directory.

This is a **source distribution**, not a signed installer or a universal DLL
for arbitrary ngscopeclient binaries. Build the host and plugin from matching
sources with the same UCRT64 compiler, dependencies and build type. The plugin
reads the host headers/import libraries and compiles its exact ImGui sources.

## Install from source

Run these commands in **MSYS2 UCRT64**, as a normal user. Install/update packages
with whatever permissions your MSYS2 installation requires. Close ngscopeclient
before replacing binaries.

The examples put both repositories under `$HOME/source`. You can use another
writable location; adjust the paths accordingly. Each numbered step continues
in the same shell. If using the earlier downloaded ZIP, its
`Tek2221-ngscopeclient-native` directory is the same source package used below.

### 1. Get this project and the tested host revision

```bash
mkdir -p "$HOME/source"
git clone https://github.com/rccipriani/ngscopeclient-tek2221.git \
    "$HOME/source/ngscopeclient-tek2221"
git clone --no-checkout https://github.com/ngscopeclient/scopehal-apps.git \
    "$HOME/source/scopehal-apps-tek2221"

project_dir="$HOME/source/ngscopeclient-tek2221/Tek2221-ngscopeclient-native"
host_dir="$HOME/source/scopehal-apps-tek2221"

git -C "$host_dir" checkout aa019065096a8c258b8c91a18453bcf7e0566274
git -C "$host_dir" submodule update --init --recursive
cd "$project_dir"
```

That pinned upstream revision is the tested baseline, rather than an assumption
that every future upstream revision is compatible. Its libscopehal baseline is
`d7461694340a4b18093b843163ebc5f776780bb0`.

### 2. Choose integrated or separate controls

For **integrated Stream Browser controls**, apply the generic hook:

```bash
cd "$project_dir"
python plugin/Apply-GUI-Hook.py "$host_dir"
```

For **DLL-only mode with a separate controls window**, skip that command.
No Tek driver files are copied into upstream in either mode.
**Do not run `Apply-Driver.py`**: that is the older built-in-driver approach.

### 3. Build ngscopeclient

```bash
cd "$project_dir"
cmake -S "$host_dir" -B "$host_dir/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF \
    -DCMAKE_SHARED_LINKER_FLAGS=-fopenmp \
    -DCMAKE_EXE_LINKER_FLAGS=-fopenmp \
    '-DCMAKE_RC_FLAGS=--preprocessor=gcc --preprocessor-arg=-E --preprocessor-arg=-xc-header --preprocessor-arg=-DRC_INVOKED'
cmake --build "$host_dir/build" -j4
```

The resource-compiler flags also accommodate MSYS2 installed under
`C:\Program Files`. See [troubleshooting](docs/TROUBLESHOOTING.md) if the compiler
cannot create temporary files.

### 4. Build, install and launch the plugin

```bash
cd "$project_dir"
bash plugin/Build-Plugin.sh "$host_dir"
bash plugin/Install-Plugin.sh "$host_dir/build/src/ngscopeclient"
bash plugin/Run-Stock.sh "$host_dir/build/src/ngscopeclient"
```

The build produces `plugin/build/Tek2221Native.dll`. Installation copies it to
`<host executable directory>/plugins/Tek2221Native.dll`, backing up an existing
plugin as `.dll.bak`. It does not replace host DLLs or alter VISA registration.

Launch through `Run-Stock.sh` from UCRT64. It sets the executable's working
directory correctly for the upstream Windows plugin loader, while the shell
supplies the MSYS2 runtime dependency paths.

For an **existing matching host build**, start at step 2, set `project_dir` and
`host_dir` to your actual paths, rebuild the host if applying the hook, then do
step 4. Keep Release/Debug consistent. The plugin defaults to Release; a custom
Debug host requires configuring the plugin to the same build type before building.

## Connect and capture

1. Connect the adapter and scope. Put the scope in **STORE**. For both channels,
   select **BOTH + ALT** on the front panel.
2. In ngscopeclient, select **Add → Oscilloscope → Connect…**.
3. Enter these settings, replacing address 5 with the actual GPIB address:

| Field | Value |
| --- | --- |
| Nickname | Your choice, e.g. `Tek2221` |
| Driver | `tek2221_native` |
| Transport | `visa` |
| Path | `GPIB0::5::INSTR` |
| Model | No fixed model selection is expected |

4. Expand the scope in Stream Browser. Add CH1's **Signal / Max** stream to a
   waveform view; add CH2 the same way when wanted. CH1 starts enabled.
5. Set probe attenuation to match the physical probe, e.g. 10×. The scope cannot
   detect a switchable probe's setting.
6. Use **Single** for one batch, **Run** for repeated transfers and **Stop** to
   stop PC transfers. Initial connection/capture takes several seconds.
7. With the hook installed, expand **Tek 2221 controls (v0.4)**. The old panel
   label remains in v0.6; it does not indicate that the older built-in driver is
   loaded. Without the hook, use the separate controls window instead.

Use a fresh `tek2221_native` connection when migrating from the old `tek2221`
built-in driver. Close WaveStar, bridge processes and other interactive VISA
sessions using the scope; the plugin opens the resource exclusively.

## Important behavior and limits

- GPIB transfers are periodic record reads, not a continuous high-speed stream.
  Expect several seconds per channel; 16-bit records take longer. Capture delay
  adds idle time **after** a completed batch.
- PC Run/Single/Stop controls transfers. It does not operate SAVE/CONT, STORE,
  analog knobs or the physical trigger configuration.
- Paired mode requires manually selecting BOTH + ALT and pressing SAVE. Leave
  controls unchanged during transfer. ALT is sequential acquisition; a paired
  read is not evidence of simultaneous channel sampling.
- Native XY decoding is implemented, but hardware validation remains outstanding.
  It requires the physical X-Y setting; software cannot switch that knob.
- Capture errors retain the previous trace and stop polling. An old visible
  trace is not confirmation of a new capture. Fix the connection and remove/re-add
  the instrument after a timeout/unplug.
- Setup slots are local ngscopeclient session data, not hardware setup memories.
  Restore is explicit; reopening a session does not automatically replay a setup.
- Reference save/erase and reset/initialization actions change scope state or
  memory. Legacy Plot targets an external plotter, not a PC screen copy.

## Documentation and project status

- [Operating guide and control reference](docs/USAGE.md)
- [Troubleshooting, diagnostics and removal](docs/TROUBLESHOOTING.md)
- [Plugin architecture and build details](Tek2221-ngscopeclient-native/plugin/README.md)
- [Optional GUI hook](Tek2221-ngscopeclient-native/plugin/GUI-HOOK.md)
- [Validation status and acceptance checklist](Tek2221-ngscopeclient-native/VALIDATION.md)
- [Standalone diagnostic protocol](Tek2221-ngscopeclient-native/PROTOCOL.md)

The v0.6 DLL and integrated sidebar have been reported working with the real
scope on a fresh upstream checkout with only the generic GUI hook applied.
Broader stress testing and some hardware modes remain outstanding; see validation.

The root v0.6 ZIP is a historical source snapshot. Git files are authoritative
for current documentation; documentation updates do not regenerate that ZIP.
Older bridge and built-in-driver files are retained for reference, not required
for this installation. License selection has not yet been recorded in this
repository; upstream ngscopeclient and third-party dependencies retain their own
licenses.
