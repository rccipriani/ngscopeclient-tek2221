# Native DLL prototype v0.5 acceptance

Completed: all seven plugin source files compiled to x86-64 Windows object files
against a pristine libscopehal header tree (no VISA enum addition), including the
C entry point, native Win32 controls, driver and transport. Pristine source diffs
remain empty. GPIB canonicalization regression tests cover collapsed, mixed and
secondary separators, invalid formats, invalid addresses and injection attempts.
Script syntax is checked. The shared native decoder/status tests are retained.

Not yet completed: actual DLL link against the host import libraries, stock
loader execution, capture/session controls in the plugin, native-window lifecycle,
tray reopen, clipboard/keyboard behavior, and hardware operations on Tachi.
C++ ABI compatibility is version/toolchain dependent; this is a prototype.

See plugin/README.md for staged current-build and pristine-host checks. Stock
waveform channels use the existing GUI; no custom ImGui controls panel is injected.
Do not claim unmodified-host success from testing only the earlier patched host.

# v0.4.4 separated status validation

The status model is directly regression tested: save slot confirmation survives
repeated captures, REF4 queries, capture/query errors and a pending new request.
A completed restore error replaces only the action result and stays visible
through subsequent successful captures. Setting writes identify their control
and count as actions. Query/action outcomes and pending-operation text are
separate; each result box has its own Copy button. Results reset on a new driver
connection rather than being serialized as historical confirmations.

Acceptance: Save slot 1, then Read REF4 and perform captures. The Last action
result must still identify Save slot 1; the REF4 response belongs under Last
query result. Restore should replace Last action result with the restore result.
Hardware actions are unchanged; GUI rendering needs confirmation on Tachi.

# v0.4.3 status panel

UI-only change: a labeled, bordered status panel with a contrasting background,
wrapped/scrolling text, error/busy/success colors and a full-text Copy button.
The Copy button is outside the busy-disabled control region. The panel header
compiles against the checked-out upstream ImGui API; rendering/clipboard behavior
need confirmation on Tachi. Native capture and command logic are unchanged.

# v0.4.2 maintenance validation

The upstream recent-menu path reconstruction used `explode`, which discards
empty fields, collapsing `GPIB0::5::INSTR` to `GPIB0:5:INSTR`. The new parser
splits only nickname/driver/transport and preserves the opaque remaining path.
Regression tests cover primary and secondary GPIB addresses, LAN, IPv6, Windows
paths, empty paths, legacy null transport and exact history round-trip.

After rebuilding, reconnect from the recent Tek2221 entry without editing the
address, then close/reopen and repeat. The saved session args and the transport
resource were already stored intact. This changes recent-menu parsing only.
Robert has confirmed that the v0.4.1 setup save/restore fix works on the real scope.

# v0.4.1 maintenance validation

The V15 `SET?` reply supplied in Robert's screenshot is transcribed into
`fixtures/setup-v15.txt`. Regression coverage verifies all 20 settings including
READOUT and NUMSWEEPS, round-trip parsing, saving a slot, changing smoothing,
readout and sweep limit, restoring each original value as separate short
commands, and detecting an ignored smoothing write. READOUT reads use `SET?`;
NUMSWEEPS:0 is the continuous-sweep value. The fixture is screenshot transcription,
not a raw byte capture. Robert subsequently confirmed save/restore working on the actual scope.

Since v0.4, Robert has confirmed paired capture, Average, envelope bounds, REF4
and scope cursor controls in ngscopeclient. Setup save/restore failed at parsing;
this maintenance release addresses that observed failure.

## Original v0.4 validation

## Already confirmed by Robert before this build

- Native 64-bit NI-VISA load, ID and CH1 binary capture on the actual 2221.
- ngscopeclient CH1 and CH2 acquisition/display, independent probe factors.
- Earlier WaveStar implementation exercised the storage controls and native
  waveform formats; that does not establish v0.4 driver/GUI acceptance.

## Completed offline for v0.4

- Actual earlier Y8/Y16 fixtures retain their established voltages/time zero.
- Synthetic 8/16-bit ENV maximum/minimum and XY X/Y values, separate axis scales,
  offsets, coupling, source labels and malformed-record rejection.
- DATA restoration on selection/query/checksum/decode failures and wrong channel
  or source. Wrong identity is rejected before a setting write.
- Paired capture requires SAVE before/between/after, rejects timing mismatch;
  XY paired capture reads one native record for both axes.
- Control readback parsing and mismatch detection, numeric bounds, injection
  rejection, query-only SAVE, strict SET parser/build order/slots, message
  validation, plot hold/abort and hardware single-sweep mode guard.
- Apply script tested against the exact released v0.3 driver and a second apply.
- C++ driver and ImGui panel compile against the real upstream headers; Windows
  x86-64 diagnostic cross-build and Windows driver syntax checks.

ENV/XY fixtures are synthetic protocol cases, not newly acquired scope records.
There is no full Windows GUI link/runtime test in this environment. The driver
async control lifecycle, renderer integration and session round-trip need Tachi.

## Acceptance on Tachi (recommended order)

1. Build, create a fresh native v0.4 connection. Confirm four channels and
   Signal / Max + Min streams, and Tek 2221 controls (v0.4) in Stream Browser.
2. Repeat working CH1/CH2 Y capture and 10× probe scaling. Only CH1 need be
   connected to the compensation output for this initial regression check.
3. Select Paired while scope is CONT: expect a clear SAVE requirement and no
   new data. Then BOTH + ALT, press SAVE, Single: expect paired success. For
   timing comparison, feed both probes from the same output with common grounds.
   The scope's ALT sampling is sequential; do not claim simultaneous sampling.
4. Read acquisition settings; change Smoothing and Average weight, confirm CRT
   and readback. Switch Sample → Average → Peak Detect at an applicable sweep
   speed. Test both channels in 8/16-bit and ENV modes; compare Max and Min.
   Underlined PEAKDET indicates smoothing on the scope.
5. Select physical XY and the panel Native XY checkbox. Feed known signals;
   confirm CH1(X)/CH2(Y) calibrations, CH2-only request and XY plot. Returning to
   normal mode requires unchecking the checkbox and changing the scope switch.
6. Explicitly save acquisition to REF4, then display REF4 sources. Confirm
   show/hide and empty-reference errors. Erase only when ready to discard it.
7. Test scope cursor controls/delta queries; ngscopeclient cursors remain local.
   Select SGL SWP physically before testing hardware Arm single sweep.
8. Save setup slot, alter one writable setting, restore it and verify readback.
   Save the ngscopeclient session, reopen and check local capture mode, delay,
   factors and slots. Reopen must not automatically replay a hardware setup.
9. Change capture delay; compare completion-to-next-start intervals (not total
   cycle time). Close all linked streams: expect no new waveform captures.
10. During a transfer, change probe factor/channel selection/Stop: stale batch
    must be discarded after DATA restoration. Exercise a panel Read during a
    capture: UI should remain responsive and the operation should serialize.
11. Optional: CRT message, next event, interface flags. Legacy plotting requires
    an appropriate external plotter/printer; do not expect an ngscope screen copy.
    Start/Auto ON must hold transfers; Abort disables auto and releases hold.
12. Unplug/reconnect test: failure should stop polling without crashing or
    publishing partial data. Close/re-add the instrument after fixing the cable.

If a test fails, retain the terminal output plus a standalone diagnostic capture
(preamble and raw CURVE), and note the physical mode/settings. Avoid diagnosing
from an old displayed trace after a reported capture error.
