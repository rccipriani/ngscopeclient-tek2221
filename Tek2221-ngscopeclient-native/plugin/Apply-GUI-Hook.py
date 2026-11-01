#!/usr/bin/env python3
"""Install only the generic Stream Browser hook; never install a built-in Tek driver."""
from pathlib import Path
import sys, shutil
if len(sys.argv)!=2: raise SystemExit('Usage: python plugin/Apply-GUI-Hook.py /path/to/scopehal-apps')
gui=Path(sys.argv[1]).resolve()/'src/ngscopeclient'
p=gui/'StreamBrowserDialog.cpp'
s=p.read_text()
include='#include "StreamBrowserPluginHook.h"'
call='\t\tif(scope) RenderStreamBrowserPluginPanels(scope);'
anchor='\t\tvector<Oscilloscope::DigitalBank> digitalBanks;'
if include not in s:
 if '#include "StreamBrowserDialog.h"' not in s: raise SystemExit('Unknown include layout; no files changed')
 s=s.replace('#include "StreamBrowserDialog.h"','#include "StreamBrowserDialog.h"\n'+include,1)
if call not in s:
 if s.count(anchor)!=1: raise SystemExit('Unknown render layout; no files changed')
 s=s.replace(anchor,call+'\n'+anchor,1)
if not p.with_suffix('.cpp.before-plugin-hook').exists(): shutil.copy2(p,p.with_suffix('.cpp.before-plugin-hook'))
shutil.copy2(Path(__file__).with_name('StreamBrowserPluginHook.h'),gui/'StreamBrowserPluginHook.h')
p.write_text(s)
print('Generic GUI hook installed. Rebuild host, then build/install plugin. No built-in driver added.')
