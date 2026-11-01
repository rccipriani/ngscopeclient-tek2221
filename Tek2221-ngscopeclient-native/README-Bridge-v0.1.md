# Tektronix 2221 → ngscopeclient, CH1 prototype v0.1

This is a first source prototype: a 32-bit Windows NI-VISA bridge and a C++
`tek2221` driver for ngscopeclient. The bridge is independent of WaveStar and
does not change its installation or registrations.

**Current status:** compiled and tested offline. Windows NI-VISA operation and
the ngscopeclient GUI still need testing on the real setup. The ZIP contains
source, build scripts and two native captures for replay, not a prebuilt
ngscopeclient executable or a drop-in DLL.

## 1. Try the bridge without hardware

Extract this folder and open ordinary PowerShell in it. No administrator rights
are needed. Build with the Windows .NET Framework compiler:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Build-Bridge.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run-Bridge.ps1 -Mode Replay8
```

Leave that terminal open. In a second PowerShell window, in the same folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Test-Bridge.ps1
```

Expect identity `TEKTRONIX,2221-BRIDGE,REPLAY,0.1`, a `T2221 1 ...` header and
`PASS: 4096 samples, 8 bits`. Close the test client before connecting ngscopeclient.
Ctrl+C stops the bridge. `-Mode Replay16` selects the actual 2048-point AVERAGE
capture; its BNC amplitude is about 0.051546875 V p-p.

Replay sends the same saved record on each capture, with a new PC receipt time.
It needs neither the GPIB adapter nor NI-VISA. `Test-Bridge.ps1` works in live mode
too, where it requests one hardware transfer.

## 2. Build ngscopeclient with the driver

The released application does not contain this driver. Use a separate source
checkout/build. The official Windows build uses **MSYS2 UCRT64**. Follow the
[upstream build guide](https://www.ngscopeclient.org/manual/GettingStarted.html#compilation)
for prerequisites; the source of its Windows instructions is
[section-gettingstarted.tex](https://github.com/ngscopeclient/scopehal-docs/blob/master/section-gettingstarted.tex).

In MSYS2 install the toolchain/dependencies (commands based on the checked-out
upstream guide, October 22, 2026):

```sh
pacman -S --needed git python mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-toolchain
pacman -S --needed mingw-w64-ucrt-x86_64-libsigc++ mingw-w64-ucrt-x86_64-yaml-cpp mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-catch mingw-w64-ucrt-x86_64-hidapi mingw-w64-ucrt-x86_64-libpng
pacman -S --needed mingw-w64-ucrt-x86_64-vulkan-headers mingw-w64-ucrt-x86_64-vulkan-loader mingw-w64-ucrt-x86_64-shaderc mingw-w64-ucrt-x86_64-glslang mingw-w64-ucrt-x86_64-spirv-tools mingw-w64-ucrt-x86_64-fftw
```

In an **MSYS2 UCRT64** shell:

```sh
cd ~
git clone --recursive https://github.com/ngscopeclient/scopehal-apps.git
```

Apply our driver. Adjust this example to the actual extracted folder:

```sh
python /c/Users/rccipriani/Downloads/Tek2221-ngscopeclient/Apply-Driver.py ~/scopehal-apps
cd ~/scopehal-apps
cmake -S . -B build -G Ninja -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF
cmake --build build -j4
./build/src/ngscopeclient/ngscopeclient.exe
```

The apply script copies two driver files and adds three lines to scopehal's
source list and registry. It checks anchors first, is safe to repeat and refuses
to overwrite different driver code. Review with `git -C lib diff`. This prototype
was compiled against scopehal-apps `aa019065096a8c258b8c91a18453bcf7e0566274`
with its scopehal submodule `d7461694340a4b18093b843163ebc5f776780bb0`.
For an exact source baseline, check out that scopehal-apps commit and run
`git submodule update --init --recursive` **before** applying the driver.

Launch the locally built application from UCRT64 so its dependency DLLs are on
PATH. The full Windows build procedure has not been executed here. A working
Vulkan-capable graphics driver is also needed, per ngscopeclient's requirements.

## 3. Connect ngscopeclient

With Replay8 running, use **Add → Oscilloscope**:

| Field | Value |
| --- | --- |
| Nickname | Tek2221 |
| Driver | `tek2221` |
| Transport | `lan` |
| Arguments | `127.0.0.1:22210` |

Start with a single acquisition. CH1 should show the replayed waveform. Run
requests repeated transfers; Single requests one. Stop stops PC polling and
discards an in-flight result when it finishes. These controls do **not** press
the scope's SAVE/CONT or change its hardware triggering. Force also requests a
single transfer of the available stored acquisition, rather than forcing a
physical trigger.

CH1 attenuation starts at **1×**. Set the channel's probe attenuation to **10×**
for the current physical probe. This changes subsequent waveform voltages once;
the bridge always sends BNC volts. At 10×, the averaged fixture should be about
0.51547 V p-p. The physical scope has no knowledge of this software factor.

## 4. Switch to the real 2221

Stop the replay bridge and close the client connection. Close WaveStar and any
NI-MAX interactive session so they do not issue competing GPIB commands.

The working setup is the NI-compatible GPIB-USB-HS adapter with **NI-488.2 and
NI-VISA**, installed/managed using NI Package Manager. NI-MAX should see
`GPIB0::5::INSTR`. The bridge is built x86 and requires the **32-bit NI-VISA
runtime**, even if ngscopeclient itself is 64-bit. TekVISA is not required.
The old TekVISA DLL caused `viOpenDefaultRM` to crash in our earlier setup;
NI's installation repair/reboot fixed that. Do not manually swap VISA DLLs.

Set the scope to STORE and CH1, connect the compensation signal and obtain a
stable waveform. Use SAMPLE or the previously working smoothed PEAKDET for Y
records; un-smoothed envelope format is not supported in this first driver.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run-Bridge.ps1 -Mode Live -Resource 'GPIB0::5::INSTR'
```

Connect the same ngscopeclient driver/transport/address. Initial connection
captures CH1 for metadata and can take several seconds. A live transfer normally
takes roughly 4–7 seconds, followed by one second idle before the next request.
The current scope record can repeat; every transfer is not necessarily a new
hardware acquisition.

The bridge console reports each capture. Latest native files and report go to
`%LOCALAPPDATA%\Tek2221Ngscope\latest`; subsequent captures overwrite those
files. They are separate from the WaveStar captures. Only one bridge instance
runs at a time, and its socket listens on loopback only.

## Supported and deferred

Supported: CH1 ACQ, calibrated Y-format unsigned binary 8-bit SAMPLE/smoothed
records and 16-bit AVERAGE records; count/checksum validation, probe scaling,
preamble time-zero mapping and explicit capture errors.

Deferred: CH2, REF4, envelope, XY, paired timing, digital acquisition controls,
configurable idle interval and automatic reconnect. Analog coupling, bandwidth,
offset, volts/div, sample rate/depth and trigger settings require the physical
front panel. The client reports the last captured values; unsupported setters
log a front-panel message and send no command. The coupling/rate/depth selectors
offer only their current value. Channel enable is a local transfer selection.
No new scope commands are introduced beyond our proven native capture path.

The initial metadata comes from a real capture; later changes are reflected
after the next completed capture. The timestamp is PC receipt time. Horizontal
position moves the CRT presentation, not the downloaded acquisition's time
reference. Offset mapping for nonzero YOFF has an offline check, but still needs
a grounded-input comparison against the real scope.

## Validation and next checks

See [VALIDATION.md](VALIDATION.md) for completed checks and their limits. First
hardware acceptance: replay in the GUI, live CH1 on the compensation signal,
10× scaling, AVERAGE mode, Single/Run/Stop and a deliberate disconnect/reconnect.
Keep our working WaveStar build available for comparison.

Distribution license selection remains open. No Tektronix or NI binaries,
WaveStar templates or upstream source are bundled.
