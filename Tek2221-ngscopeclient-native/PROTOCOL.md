# Tek2221 bridge protocol v1

ASCII LF commands on a single TCP connection at `127.0.0.1:22210`. Loopback only;
one client at a time. No VISA/COM registration and no WaveStar dependency.

| Command | Response |
| --- | --- |
| `*IDN?` | `TEKTRONIX,2221-BRIDGE,REPLAY,0.1` (or `GPIB` in live mode), LF |
| `META?` | Metadata header, LF. Captures once if no record is cached. |
| `CAPTURE?` | Fresh metadata header, LF, then exactly `count * 4` bytes. |

Header: `T2221 1 count interval_fs trigger_phase_fs range_volts coupling bits`

The payload is IEEE754 float32 little endian, in **BNC volts**, without a trailing
newline. Probe attenuation is applied in the client driver, once. Range is eight
vertical divisions in BNC volts. Coupling is AC, DC or GND. Bits is 8 or 16.
Errors return `ERR message\n` and no binary payload. No arbitrary GPIB passthrough.

Sample time is `i * interval_fs + trigger_phase_fs`, with
`trigger_phase_fs = -PT.OFF * XINCR * 1e15`. Voltage is
`(unsigned_code - midpoint - YOFF) * YMULT`, where midpoint is 128 or 32768.
Native 16-bit words are MSB first; the bridge converts these to float32.
The capture timestamp assigned by the driver is PC receipt time, not the scope's
trigger time. No assertion of trigger synchronization is made.

Each live request checks `ID?`, retains `DATA?`, selects ACQ/CH1/BINARY, fetches
`WFMPRE?` and `CURVE?`, validates count/checksum, and attempts restoration of the
original DATA selection in `finally`. Restoration failure rejects the capture.
SAVE/CONT, acquisition mode, coupling, volts/div and time/div are not changed.

The first prototype accepts calibrated Y-format records only. ENV, XY, external
clock, uncalibrated volts and unknown offsets fail explicitly. A capture error
stops client polling; restart after correcting the scope setting. Reconnect the
bridge after a short/partial TCP payload, since framing may then be lost.
