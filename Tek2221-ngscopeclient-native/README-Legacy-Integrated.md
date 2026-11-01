> Historical v0.4.x source-integration instructions. For v0.6 installation, use the [repository README](../README.md). Do not run Apply-Driver.py for the DLL installation.

# Tektronix 2221 → ngscopeclient v0.4.4: native 64-bit VISA

**64-bit ngscopeclient → System32/visa64.dll → NI-VISA / NI-488.2 → GPIB → 2221**

This is a source package for the working ngscopeclient checkout, with a native
C++ driver and a small custom Stream Browser panel. No bridge process, WaveStar,
TekVISA, NI SDK headers or import libraries are needed for the native connection.
The old C# LAN bridge remains an optional CH1-only comparison path.

## v0.4.4 persistent action results

The former shared status box is split into **Capture status**, **Last action
result**, and **Last query result**, each with its own Copy button. Save/Restore
confirmations and setting-write results remain visible until another action or
setting write finishes. Captures, explicit Read queries and their errors cannot
overwrite an action result. Action and query results include the operation name;
pending operations appear separately and preserve the previous completed result.
Result colors use the operation's success/error outcome rather than guessing
from the text. These are per-connection results, not a persistent history log.
This update includes the setup and address fixes. Apply/rebuild as below; model
and panel headers still say v0.4, and the current channel layout can be retained.

## v0.4.3 status display

The current status now appears in a bordered, contrasting panel with wrapped
text and vertical scrolling for long replies. Errors use red, busy operations
amber, and recognized capture/setup successes green. A Copy button copies the
complete original status text and remains available while the scope is busy.
v0.4.4 separates this panel into three categories, preserving action results
while captures and queries continue. The update includes the setup and address fixes.
Apply/build as below; no new connection/channel layout is required. Model and
panel headings still say v0.4. GUI rendering needs confirmation on Tachi.

## v0.4.2 recent-instrument address fix

Reconnecting from the recent-instrument menu previously collapsed VISA double
colons, displaying `GPIB0:5:INSTR` instead of `GPIB0::5::INSTR`. The transport
saved the original address correctly; the upstream menu split it with a helper
that discarded empty fields. This update splits only the three history metadata
fields and preserves the remaining transport path verbatim. LAN/IPv6/secondary
GPIB addresses and legacy null paths are covered by regression tests. Existing
correct saved history entries work without manually adding colons again.

Close ngscopeclient, apply/build as below, then reconnect from Add → Oscilloscope
→ the recent Tek2221 entry. You do not need a fresh channel layout. The model and
panel still display v0.4. This maintenance build includes the v0.4.1 setup fix.

## v0.4.1 setup save/restore fix

The real V15 `SET?` response includes `READOUT ON` and `ACQUISITION NUMSWEEPS:0`.
v0.4 omitted both and rejected Save with "Invalid setup argument". This update
preserves the CRT readout setting and the sweep limit (0 = continuous, 1–2047
finite acquisitions), verifies readback, and restores short individual commands
rather than sending one long setup line. No arbitrary command replay is enabled.
The exact reply transcribed from the scope screenshot is a regression fixture.
READOUT is queried through `SET?`, without assuming an undocumented `READOUT?`.

To update v0.4, close ngscopeclient and run the same apply/build commands below.
The script recognizes the released v0.4 source. The model and panel still display
v0.4; a fresh channel layout is not required for this maintenance update. Repeat
Save slot 1 → change Smoothing → Restore slot 1 → Read Smoothing.

## Upgrade from v0.3

Close ngscopeclient, WaveStar, the bridge and NI-MAX interactive sessions. Extract
this package into a new folder. In **MSYS2 UCRT64**, from that extracted folder:

```sh
cd /c/Users/rccipriani/Downloads/Tek2221-ngscopeclient-native
python ./Apply-Driver.py /c/Users/rccipriani/source/scopehal-apps
cd /c/Users/rccipriani/source/scopehal-apps
cmake --build build -j4
./build/src/ngscopeclient/ngscopeclient.exe
```

Keep your existing build directory: it retains the OpenMP and windres fixes for
MSYS2 installed under Program Files. The apply script recognizes the original
v0.1, released v0.2, v0.3, v0.4 and v0.4.1 sources, backs up changed files, and refuses to
overwrite unrecognized driver edits. Applying it again is safe.

Create a **fresh connection** for this release (the channel/stream list changed):

| Field | Value |
| --- | --- |
| Menu | Add → Oscilloscope → Connect… |
| Nickname | Tek2221 |
| Driver | `tek2221` |
| Model | **Tektronix 2221 (native 64-bit VISA v0.4, full storage)** |
| Transport | `visa` |
| Path | `GPIB0::5::INSTR` |

Use double colons in the VISA address. Initial connection reads CH1 for metadata
and takes several seconds. Expand the instrument's **Tek 2221 controls (v0.4)**
node in Stream Browser. Start with Single, then right-click the time ruler →
Autofit. Set each physical probe's Attenuation (e.g. 10×) in the normal channel
settings. The scope does not detect a switchable probe's setting.

## Waveforms and capture modes

| Source | Meaning |
| --- | --- |
| CH1 / CH2 | Acquisition memory, enabled by adding their streams to a view/filter |
| REF4 CH1 / REF4 CH2 | Stored reference memory, not continuous acquisition |
| Signal / Max | Normal Y signal, or the upper bound of an ENV record |
| Min | Lower bound of ENV; identical to Signal for ordinary Y and XY records |

CH1 starts enabled; other sources start disabled. Expand a channel and drag
**Signal / Max** into the graph. Add CH2 the same way. Select **STORE, BOTH + ALT**
on the scope for dual-channel captures. Remove a channel from all views/filters
to stop transferring it. Reading an empty or incompatible REF4 produces an error.
REF4 uses the corresponding physical channel's configured probe factor; it does
not retain historical probe factors from when the reference was saved.

Both unsigned 8-bit and MSB-first 16-bit binary samples are decoded. ENV preserves
both bounds at the original bin spacing, with no averaging or decimation.
The default scope probe-adjust square wave is about 0.5 V p-p at the tip; with a
10× probe its uncorrected BNC amplitude is about 0.05 V p-p.

**Live:** sequential transfers from the enabled sources. A common PC timestamp
groups the batch; it does not imply simultaneous acquisition. Both new results
are discarded if any requested transfer fails.

**Paired:** requires **BOTH + ALT**, then manually press **SAVE** and leave the
controls unchanged. Both ACQ channels are read even if only one is displayed.
SAVE is checked before, between and after the reads, and again before publishing;
count, sample spacing, time zero and format must match. A failed check discards
the new pair and stops polling. ALT channel sampling is sequential, not
simultaneous. SAVE-state checks cannot detect every possible front-panel change
(e.g. SAVE off and back on between checks); leave the controls alone during reads.
Reference-only reads do not require SAVE.

**Native XY:** select the scope's physical X-Y switch in STORE mode and check
**Native XY (physical X-Y switch)** in the panel. A single native XY record is
split into CH1 (X) and CH2 (Y), each with its own calibration and probe factor.
This also works for REF4 when it contains XY data. It does not switch the scope
into XY remotely or create an XY graph automatically: use ngscopeclient's XY
view/filter with the two components. The checkbox also ensures a CH2-only request
reads the shared XY record through CH1. If CH1 is already requested, XY packets
are recognized automatically. An ordinary Y record while the checkbox is set
produces an explicit error.

Run repeatedly transfers stored records; **Delay after capture** (0–60 seconds,
default 1) is idle time *after* the whole batch finishes. It cannot shorten GPIB
transfer time. Expect several seconds per channel; 16-bit records take longer.
Single transfers one batch. Stop discards an in-flight batch once the current
transaction completes/restores DATA. None of these buttons operates SAVE/CONT,
forces a hardware trigger or changes the scope's analog knobs.

## Tek 2221 controls panel

Scope queries/writes run in a background task serialized with capture. The panel
remains responsive; controls show a busy message until the request completes.
Values are queried on explicit **Read**, not polled continuously. Dropdown labels
use Sample, Peak Detect, Average and AccPeak rather than command abbreviations.
Readback is checked for writable settings; a transport success alone is not
reported as acceptance.

| Section | Features |
| --- | --- |
| Acquisition | Smoothing, vectors, average weight, sweep limit, trigger count, current mode, HSR/LSR/repetitive/roll/scan mode |
| Cursors | Scope cursor channel, target, selection and position; these are separate from ngscopeclient cursors |
| Status | SAVE, STORE, reference state/source, trigger state, horizontal/vertical/channel settings, reported probe factors, points/sweeps/trigger position, scope delta V/T, identity, command headers and writable setup text |
| Interface | Service requests, operation-complete events, CRT readout, verbose replies |
| Actions | Reset modes, initialize defaults, arm hardware single sweep, save/show/hide/erase REF4, save/restore setup slot, show/clear CRT message, consume next event |
| Legacy plot | Format, graticule, pen speed, automatic plotting, start and abort |

Hardware **SGL SWP must be selected physically** before Arm hardware single sweep.
SAVE and STORE remain read-only status values. Analog coupling, gain, position,
timebase, bandwidth limit and physical trigger settings remain front-panel
controls. The normal ngscopeclient analog setting widgets cannot override them.
Native capture requires verbose replies (`LONG ON`); `LONG OFF` is disabled.

Setup slots 1–10 are **local session slots**, not hardware memories. Save/restore
uses a strict allowlist of writable `SET?` fields and verifies readback. No raw
command editor/replay is exposed. Slots exclude waveform memory and physical
knob settings. Automatic plotting must be off before saving a setup. Save the
ngscopeclient session to persist slots, paired/XY selection, capture delay and
probe factors. Reopening a session restores those local preferences without
replaying saved setup slots to the scope; Restore is an explicit action.

Legacy plotting addresses an external plotter/printer, not screen capture.
Start or automatic plotting pauses driver capture and suppresses driver identity
polls. Use **Abort plot** to send abort, disable automatic plotting and release
the hold; then Run/Single to resume. Displayed plot state is the driver's hold,
not a verified external printer completion. Scope status/event replies remain raw
where no validated enum mapping is available.

## NI installation and troubleshooting

Tested hardware is an NI-compatible **GPIB-USB-HS clone** with NI-488.2 and NI-VISA
installed through **NI Package Manager (2026 Q3 on Tachi)**. NI-MAX should see the
adapter and scope. Use a normal user terminal for builds/running. TekVISA is not
needed. Earlier 32-bit TekVISA caused access violations; repairing the NI stack
and rebooting resolved that. Do not manually copy VISA DLLs into the app folder.
The native driver loads the standard 64-bit dispatcher at
`C:\Windows\System32\visa64.dll`; that dispatcher routes to the configured provider.

If compiling reports a temporary-directory permission error with MSYS2 under
Program Files, set a writable directory in the build terminal:

```sh
mkdir -p /c/Users/rccipriani/AppData/Local/Temp/msys2
export TMPDIR=/c/Users/rccipriani/AppData/Local/Temp/msys2
export TMP="$TMPDIR"
export TEMP="$TMPDIR"
```

Existing build-cache fixes, if needed for a fresh build, are:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF \
  -DCMAKE_SHARED_LINKER_FLAGS=-fopenmp -DCMAKE_EXE_LINKER_FLAGS=-fopenmp \
  '-DCMAKE_RC_FLAGS=--preprocessor=gcc --preprocessor-arg=-E --preprocessor-arg=-xc-header --preprocessor-arg=-DRC_INVOKED'
```

The resource is opened exclusively. Close competing instrument applications.
Capture errors retain the previous displayed data, stop polling and appear in
the panel/terminal; they do not replace the trace with zeros. After a timeout or
unplug, close/re-add the connection to recover the session; no automatic bus
reset, device clear or reconnect is issued. Stop/disconnect cannot instantly
interrupt a native GPIB call; allow DATA restoration to finish.

## Standalone diagnostic and offline tests

In this package folder, from UCRT64 (close ngscopeclient first):

```sh
bash ./Build-Native-Check.sh
./Tek2221VisaCheck.exe --load-only
./Tek2221VisaCheck.exe --id 'GPIB0::5::INSTR'
./Tek2221VisaCheck.exe --capture 'GPIB0::5::INSTR' native-results
./Tek2221VisaCheck.exe --capture-ch2 'GPIB0::5::INSTR' native-results-ch2
# BOTH + ALT, then SAVE first:
./Tek2221VisaCheck.exe --pair 'GPIB0::5::INSTR' paired-results
# Only after saving a compatible reference:
./Tek2221VisaCheck.exe --ref4 'GPIB0::5::INSTR' ref4-results
./Tek2221VisaCheck.exe --ref4-ch2 'GPIB0::5::INSTR' ref4-results-ch2
```

Output folders contain the original preamble, checksum-validated raw response
and calibrated BNC-voltage CSV. ENV CSV includes the minimum; raw XY CSV includes
both Y and X voltages. Paired output has CH1/CH2 subfolders. Probe factors are not
applied by this diagnostic. The GUI does not write capture folders automatically;
use ngscopeclient's session/export facilities to retain traces.

```sh
bash ./Run-Offline-Tests.sh
```

Offline coverage includes Y8/Y16 fixtures, synthetic ENV/XY8/16, REF4, checksum,
source/channel mismatch, scaling, DATA restoration failures, SAVE guards, timing
mismatch, strict settings and setup parsing, readback mismatch, messages, plot
hold/abort and single-sweep guards. See `VALIDATION.md` for on-scope acceptance.
The new v0.4 GUI/hardware features are not yet validated on Tachi.

The upstream source baseline is scopehal-apps
`aa019065096a8c258b8c91a18453bcf7e0566274`, scopehal
`d7461694340a4b18093b843163ebc5f776780bb0`. The apply script changes source build
registration plus the GUI render hook and recent-instrument path parser, and copies the supplied files.
It does not install into Program Files or modify VISA registration. This is a
source integration package, not a signed Windows installer. Distribution license
selection remains open; no third-party binaries or WaveStar templates are bundled.

The optional old bridge is documented in `README-Bridge-v0.1.md`; do not run it
against the same scope while the native connection is open.
