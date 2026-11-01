# v0.6 validation status

## Reported hardware results

On Windows 11 x64 with MSYS2 UCRT64, NI-488.2/NI-VISA (2026 Q3), an NI-compatible
GPIB-USB-HS clone and a V15 Tektronix 2221:

- Native 64-bit VISA identification and waveform capture succeeded.
- The DLL loaded, connected, queried controls and captured after matching the
  host/plugin Release build type. A mismatched Vulkan dispatcher layout initially
  caused an assertion; the documented v0.6 build defaults address that mismatch.
- The integrated sidebar was reported working on a fresh pinned upstream
  checkout with only the generic GUI hook applied.
- Earlier native-driver testing confirmed CH1/CH2, probe attenuation, paired
  SAVE guards, Average/16-bit capture, envelope bounds, REF4, scope cursors,
  setup save/restore, session persistence and several error/recovery paths.
  Those results are inherited evidence, not a claim that every path was repeated
  after the DLL/sidebar packaging change.

The separate-window DLL path also achieved working capture during development.
A complete native-window lifecycle/clipboard/tray regression checklist has not
been recorded. Native XY has offline decoding coverage but no completed hardware
acceptance recorded here. Legacy plotting requires suitable external hardware.
General compatibility with arbitrary upstream builds, other ROMs or the 2221A
has not been established.

## Offline and compile checks

Decoder/control/status/resource regression tests cover the cases listed below.
The DLL sidebar, generic hook and changed driver passed Windows cross-compilation
against the checked-out upstream headers. The GUI-hook installer was applied
twice to check idempotence. These checks do not replace Windows runtime tests.

## Recommended v0.6 acceptance

1. Confirm the plugin log, create a fresh `tek2221_native`/`visa` connection and
   capture CH1; add CH2 with the scope on STORE, BOTH + ALT.
2. In integrated mode, expand Tek controls in Stream Browser and verify no
   separate window. In DLL-only mode, verify the window and tray reopen behavior.
3. Read one scope setting, make a supported change, verify readback and CRT.
   Confirm explicit reads do not overwrite the last completed action result.
4. Run/Single/Stop, link/unlink channels, change probe attenuation/capture delay,
   and verify no new captures after all streams are unused.
5. Exercise Paired with SAVE off (expected rejection), then SAVE on (success).
   Leave physical controls unchanged during transfer.
6. Save/reopen a session, remove/re-add the instrument, and close the app while
   idle and while busy. Confirm no crash or orphan controls window.
7. Repeat Average/ENV, REF4 and setup actions; use known signals for XY testing.
8. Test unplug/timeout recovery without interpreting a retained old trace as new
   data. Fix the connection, then remove/re-add the instrument.

For detailed inherited test history and the longer hardware checklist, see below.
Older "needs confirmation" notes describe that historical stage, not a reversal
of the current results above.

---

# Historical validation notes

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
