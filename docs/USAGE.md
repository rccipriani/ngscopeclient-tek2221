# Using the Tektronix 2221 driver

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
**Native XY (physical X-Y switch)** in the sidebar or separate controls window. A single native XY record is
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

## Tek controls

Scope queries/writes run in a background task serialized with capture. The panel
remains responsive; controls show a busy message until the request completes.
Values are queried on explicit **Read**, not polled continuously. The integrated sidebar uses friendly dropdown labels such as Sample, Peak Detect,
Average and AccPeak. The separate window uses command abbreviations
(SAM, PEA, AVE and ACC).
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


See [installation](../README.md) and [troubleshooting](TROUBLESHOOTING.md).
