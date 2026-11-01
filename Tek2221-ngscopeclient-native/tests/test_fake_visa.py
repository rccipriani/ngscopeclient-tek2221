"""Linux/Mono-only safety test; requires fake_visa.c built as a shared library.

Uses a private DLL map; never loads a system VISA library. Reuses test_replay helpers.
TEK2221_BRIDGE_RUNNER must be set for Mono, and --exe points to the test binary.
"""
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
import json
import argparse

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--exe',type=Path,required=True)
p.add_argument('--fake-library',type=Path,required=True)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    d=Path(temp)
    exe=d/'bridge.exe'
    shutil.copy2(a.exe,exe)
    # Only this private test executable is redirected to the fake.
    (d/'bridge.exe.config').write_text('<configuration><dllmap dll="visa32.dll" target="'+str(a.fake_library.resolve())+'" /></configuration>')
    env=os.environ.copy()
    env['TEK_TEST_FIXTURE']=str(root/'fixtures/y8')
    env['TEK_TEST_LOG']=str(d/'visa.log')
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1]
    with (d/'bridge.log').open('w+') as log:
        proc=subprocess.Popen(json.loads(env['TEK2221_BRIDGE_RUNNER'])+[str(exe),'--port',str(port),'--work',str(d/'work')],env=env,stdout=log,stderr=subprocess.STDOUT)
        try:
            end=time.monotonic()+10
            while True:
                try: conn=socket.create_connection(('127.0.0.1',port),timeout=1);break
                except OSError:
                    if proc.poll() is not None or time.monotonic()>end:
                        log.seek(0);raise AssertionError(log.read())
                    time.sleep(.05)
            with conn:
                conn.sendall(b'CAPTURE?\n')
                header=bytearray()
                while not header.endswith(b'\n'):
                    b=conn.recv(1);assert b;header.extend(b)
                assert header.startswith(b'T2221 1 4096 '),header
                remaining=4096*4
                while remaining:
                    b=conn.recv(remaining);assert b;remaining-=len(b)
            commands=(d/'visa.log').read_text().splitlines()
            assert commands==['GPIB0::5::INSTR','ID?','DATA?',
                'DATA SOURCE:ACQ,CHANNEL:CH1,ENCDG:BINARY','WFMPRE?','CURVE?',
                'DATA SOURCE:ACQ,TARGET:REF4,CHANNEL:CH2,ENCDG:ASCII'],commands
            print('PASS: live bridge path used only ID/DATA/WFMPRE/CURVE and restored original CH2/ASCII DATA settings (fake VISA)')
        finally:
            proc.terminate();proc.wait(timeout=5)
