"""Exercise the actual bridge over TCP using native 2221 capture fixtures.

Build the bridge first. Windows: python tests/test_replay.py
Other hosts may set TEK2221_BRIDGE_RUNNER to a JSON command prefix (e.g. Mono).
No instrument access occurs in this test.
"""
import argparse
import json
import math
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import shutil

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--exe', type=Path, default=ROOT/'Tek2221NgBridge.exe')
args = p.parse_args()
runner = json.loads(os.environ.get('TEK2221_BRIDGE_RUNNER','[]'))

def recv_exact(conn, n):
    result = bytearray()
    while len(result) < n:
        part = conn.recv(n-len(result))
        assert part, 'Premature EOF'
        result.extend(part)
    return result

def line(conn):
    result = bytearray()
    while True:
        b = recv_exact(conn,1)
        if b == b'\n': return result.decode('ascii')
        result.extend(b)
        assert len(result) < 4096

def query(conn, text):
    conn.sendall(text.encode('ascii')+b'\n')
    return line(conn)

def record(conn):
    header = query(conn,'CAPTURE?').split()
    assert header[:2] == ['T2221','1'], header
    assert len(header) == 8
    count = int(header[2])
    assert 0 < count <= 4096
    data = struct.unpack('<'+'f'*count, recv_exact(conn,4*count))
    assert all(math.isfinite(x) for x in data)
    return header, data

for fixture, count, bits, dt, phase, expected_pp in [
    ('y8',4096,8,5000000000,-2560000000000, None),
    ('y16',2048,16,4000000000,-2048000000000,0.051546875),
]:
    with tempfile.TemporaryDirectory() as temp:
        sample = Path(temp)/fixture
        shutil.copytree(ROOT/'fixtures'/fixture,sample)
        with socket.socket() as reserve:
            reserve.bind(('127.0.0.1',0)); port=reserve.getsockname()[1]
        log = open(Path(temp)/'log.txt','w+')
        proc = subprocess.Popen(runner+[str(args.exe.resolve()),'--replay',str(sample),'--port',str(port)],
                                stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic()+10
            while True:
                try:
                    conn=socket.create_connection(('127.0.0.1',port),timeout=1)
                    break
                except OSError:
                    if proc.poll() is not None or time.monotonic()>deadline:
                        log.seek(0); raise AssertionError(log.read())
                    time.sleep(.05)
            with conn:
                conn.settimeout(5)
                assert query(conn,'*IDN?') == 'TEKTRONIX,2221-BRIDGE,REPLAY,0.1'
                meta=query(conn,'META?')
                h,data=record(conn)
                assert ' '.join(h)==meta
                assert int(h[2])==count and int(h[7])==bits
                assert int(h[3])==dt and int(h[4])==phase
                assert int(h[4])+(-phase//dt)*dt==0, 'PT.OFF must map to t=0'
                if expected_pp is not None:
                    assert math.isclose(max(data)-min(data),expected_pp,abs_tol=1e-7)
                else:
                    assert math.isclose(data[0],-.0288,abs_tol=1e-7), data[0]
                assert query(conn,'ACQ SMO:ON').startswith('ERR '), 'No arbitrary hardware passthrough'
                # Corrupt a native block. No stale samples or binary bytes may follow ERR.
                raw=(sample/'curve-response.bin').read_bytes()
                bad=bytearray(raw); bad[bad.index(ord('%'))+3]^=1
                (sample/'curve-response.bin').write_bytes(bad)
                assert query(conn,'CAPTURE?').startswith('ERR Curve checksum mismatch')
                assert query(conn,'*IDN?').endswith(',0.1'), 'Error desynchronized stream'
                (sample/'curve-response.bin').write_bytes(raw)
                _,again=record(conn)
                assert again==data
            # A new client can connect after the previous one closes.
            with socket.create_connection(('127.0.0.1',port),timeout=3) as conn:
                assert query(conn,'*IDN?').endswith(',0.1')
            print('PASS',fixture,': TCP framing, native decode, scaling, time zero, checksum rejection, recovery, reconnect')
        finally:
            proc.terminate()
            proc.wait(timeout=5)
            log.close()
