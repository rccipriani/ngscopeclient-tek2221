"""Install/update the Tek2221 driver and native VISA transport in a scopehal-apps checkout."""
from pathlib import Path
import argparse
import hashlib
import shutil
import datetime

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('checkout',type=Path)
a=p.parse_args()
root=a.checkout.resolve()/'lib'/'scopehal'
source=Path(__file__).resolve().parent/'driver'
old_hashes={
 'Tek2221Oscilloscope.cpp':'916dc2ff91bd652cfaccf70abc96f1428eb36d312b679702c6f912bcaf1f4a30',
 'Tek2221Oscilloscope.h':'0b80f1f5d933c671af67bc2d2b03d3d7f386aa6a3427e6375098d49898379cb9',
}

v02_hashes={'Tek2221Oscilloscope.h': '51910810e8aa01ad84e5e3eb021f1dbedfa6b239a2b32c8774724e80c2226c1e', 'SCPIVISATransport.cpp': '9946b4ac8f830d3b71317a6968d520243c8541058191ed4f49350fccc4d39ae6', 'SCPIVISATransport.h': '28c72022df544ae7ebe4ab8d8771bbf908f8b2e0ec31aa1fa0f53b42254a80ed', 'Tek2221Native.cpp': 'eb20349bf92f622c900a096244b1bbfd05d5e97affb238595a5ffb0088a1d898', 'Tek2221VISA.h': 'd9d03e32333aae2de65e5242d47d4ecde2e4d3392f26118724cc61e9d2ac4764', 'Tek2221Native.h': '391a286a39747b098d4c0f0b6b98c90b9da4328eb63d9bdbb35462f1d90ec4d9', 'Tek2221Oscilloscope.cpp': '2393382cb065505afe650d2bf37fce484c4c53cb63f87960937da1ff075e469a', 'Tek2221VISA.cpp': '1f0910a0d7b2c3c3afc11fd58f3c0c2b3c4baf6200aaae7467d8469d9d684f37'}

v03_hashes={'Tek2221VISA.cpp': '1f0910a0d7b2c3c3afc11fd58f3c0c2b3c4baf6200aaae7467d8469d9d684f37', 'SCPIVISATransport.h': '28c72022df544ae7ebe4ab8d8771bbf908f8b2e0ec31aa1fa0f53b42254a80ed', 'Tek2221Oscilloscope.cpp': 'e60d183c8e2ed053294f4d6cf24047705cd72de6af9b25bfbbdba402fc3e49dd', 'Tek2221Native.h': '164c68973c9fd1116f7409c6bff710c8ee19754932dc588159750439170c2064', 'Tek2221Native.cpp': '8c93fdff5619de2656dd9851375e0bbadeab370fe88cb88a5405261b86246808', 'Tek2221VISA.h': 'd9d03e32333aae2de65e5242d47d4ecde2e4d3392f26118724cc61e9d2ac4764', 'SCPIVISATransport.cpp': '9946b4ac8f830d3b71317a6968d520243c8541058191ed4f49350fccc4d39ae6', 'Tek2221Oscilloscope.h': '542019fcc1151cc2c8449a0e1e6cf3eb6ede1554ed717f5ce1f3dcb534c25b8a'}

v04_hashes={"Tek2221VISA.cpp": "1f0910a0d7b2c3c3afc11fd58f3c0c2b3c4baf6200aaae7467d8469d9d684f37", "SCPIVISATransport.h": "28c72022df544ae7ebe4ab8d8771bbf908f8b2e0ec31aa1fa0f53b42254a80ed", "Tek2221Controls.cpp": "dce30bd4d0ea4d20de0e9cda3b6b148746d9cbce3b53d6c73d73229a9d65be5d", "Tek2221Oscilloscope.cpp": "41447f106bda347dd12830cff88a69084e5d2b68e30a328ed84123cc58901bd5", "Tek2221Native.h": "441c42e3a168b5254473158b1edc2d7f34fe28a7290c693a25d9da1f00aff025", "Tek2221Native.cpp": "a159940e0772bddb945f2badaf0f4c1e2cc1d114f65e5c413311698def349bc3", "Tek2221VISA.h": "d9d03e32333aae2de65e5242d47d4ecde2e4d3392f26118724cc61e9d2ac4764", "Tek2221Controls.h": "f4191a0076711cd1e2fe152237ad28ff915cfb98b493329e64072b75578f238c", "SCPIVISATransport.cpp": "9946b4ac8f830d3b71317a6968d520243c8541058191ed4f49350fccc4d39ae6", "Tek2221Oscilloscope.h": "604f60abf26b9184739444445767be3f43b43008ccb25fdd78571fa6341d0d51"}

v041_hashes={'Tek2221Controls.cpp': '557887ac494eadb2f1cae57a952b35f0248d278b27765fba2ab4c61908cf3ca7', 'Tek2221Native.cpp': 'a159940e0772bddb945f2badaf0f4c1e2cc1d114f65e5c413311698def349bc3', 'Tek2221Native.h': '441c42e3a168b5254473158b1edc2d7f34fe28a7290c693a25d9da1f00aff025', 'Tek2221Oscilloscope.cpp': '41447f106bda347dd12830cff88a69084e5d2b68e30a328ed84123cc58901bd5', 'Tek2221VISA.h': 'd9d03e32333aae2de65e5242d47d4ecde2e4d3392f26118724cc61e9d2ac4764', 'SCPIVISATransport.cpp': '9946b4ac8f830d3b71317a6968d520243c8541058191ed4f49350fccc4d39ae6', 'Tek2221Oscilloscope.h': '604f60abf26b9184739444445767be3f43b43008ccb25fdd78571fa6341d0d51', 'SCPIVISATransport.h': '28c72022df544ae7ebe4ab8d8771bbf908f8b2e0ec31aa1fa0f53b42254a80ed', 'Tek2221Controls.h': 'f4191a0076711cd1e2fe152237ad28ff915cfb98b493329e64072b75578f238c', 'Tek2221VISA.cpp': '1f0910a0d7b2c3c3afc11fd58f3c0c2b3c4baf6200aaae7467d8469d9d684f37'}

v043_hashes={'Tek2221Controls.cpp': '557887ac494eadb2f1cae57a952b35f0248d278b27765fba2ab4c61908cf3ca7', 'Tek2221Native.cpp': 'a159940e0772bddb945f2badaf0f4c1e2cc1d114f65e5c413311698def349bc3', 'Tek2221Native.h': '441c42e3a168b5254473158b1edc2d7f34fe28a7290c693a25d9da1f00aff025', 'Tek2221Oscilloscope.cpp': '41447f106bda347dd12830cff88a69084e5d2b68e30a328ed84123cc58901bd5', 'Tek2221VISA.h': 'd9d03e32333aae2de65e5242d47d4ecde2e4d3392f26118724cc61e9d2ac4764', 'SCPIVISATransport.cpp': '9946b4ac8f830d3b71317a6968d520243c8541058191ed4f49350fccc4d39ae6', 'Tek2221Oscilloscope.h': '604f60abf26b9184739444445767be3f43b43008ccb25fdd78571fa6341d0d51', 'SCPIVISATransport.h': '28c72022df544ae7ebe4ab8d8771bbf908f8b2e0ec31aa1fa0f53b42254a80ed', 'Tek2221Controls.h': 'f4191a0076711cd1e2fe152237ad28ff915cfb98b493329e64072b75578f238c', 'Tek2221VISA.cpp': '1f0910a0d7b2c3c3afc11fd58f3c0c2b3c4baf6200aaae7467d8469d9d684f37'}

v044_hashes={'SCPIVISATransport.cpp': '9946b4ac8f830d3b71317a6968d520243c8541058191ed4f49350fccc4d39ae6', 'SCPIVISATransport.h': '28c72022df544ae7ebe4ab8d8771bbf908f8b2e0ec31aa1fa0f53b42254a80ed', 'Tek2221Controls.cpp': '557887ac494eadb2f1cae57a952b35f0248d278b27765fba2ab4c61908cf3ca7', 'Tek2221Controls.h': 'f4191a0076711cd1e2fe152237ad28ff915cfb98b493329e64072b75578f238c', 'Tek2221Native.cpp': 'a159940e0772bddb945f2badaf0f4c1e2cc1d114f65e5c413311698def349bc3', 'Tek2221Native.h': '441c42e3a168b5254473158b1edc2d7f34fe28a7290c693a25d9da1f00aff025', 'Tek2221Oscilloscope.cpp': '774d61323f699798ba94ab784000172c264dbb3aff173bb96cb52567298164e1', 'Tek2221Oscilloscope.h': '137af15bde32ba556666401bf067f25a3f4b4472e1a49fe18dacdbfab298e77e', 'Tek2221Status.h': '79e770c33312894de33ad7c8722c71b020a0e9969bd1ff2c2b306af1e0d3943f', 'Tek2221VISA.cpp': '1f0910a0d7b2c3c3afc11fd58f3c0c2b3c4baf6200aaae7467d8469d9d684f37', 'Tek2221VISA.h': 'd9d03e32333aae2de65e5242d47d4ecde2e4d3392f26118724cc61e9d2ac4764'}

def insert_once(text,addition,anchor):
    if addition in text: return text
    if text.count(anchor)!=1: raise SystemExit('Upstream layout changed: '+repr(anchor))
    return text.replace(anchor,addition+'\n'+anchor,1)

required=['CMakeLists.txt','scopehal.cpp','SCPIInstrument.h','SCPIInstrument.cpp']
if not all((root/n).is_file() for n in required): p.error('Expected recursive scopehal-apps checkout')
texts={n:(root/n).read_text() for n in required}
cpp_names=['Tek2221Oscilloscope.cpp','Tek2221Native.cpp','Tek2221VISA.cpp','SCPIVISATransport.cpp','Tek2221Controls.cpp']
for name in cpp_names:
    texts['CMakeLists.txt']=insert_once(texts['CMakeLists.txt'],'\t'+name,'\tTektronixOscilloscope.cpp')
texts['scopehal.cpp']=insert_once(texts['scopehal.cpp'],'#include "Tek2221Oscilloscope.h"','#include "TektronixOscilloscope.h"')
texts['scopehal.cpp']=insert_once(texts['scopehal.cpp'],'#include "SCPIVISATransport.h"','#include "TektronixOscilloscope.h"')
texts['scopehal.cpp']=insert_once(texts['scopehal.cpp'],'\tAddDriverClass(Tek2221Oscilloscope);','\tAddDriverClass(TektronixOscilloscope);')
texts['scopehal.cpp']=insert_once(texts['scopehal.cpp'],'#ifdef _WIN32\n\tAddTransportClass(SCPIVISATransport);\n#endif','\tAddTransportClass(SCPISocketTransport);')
if 'TRANSPORT_VISA' not in texts['SCPIInstrument.h']:
    anchor='\tTRANSPORT_LXI\n'
    if texts['SCPIInstrument.h'].count(anchor)!=1: raise SystemExit('Transport enum layout changed')
    texts['SCPIInstrument.h']=texts['SCPIInstrument.h'].replace(anchor,'\tTRANSPORT_LXI,\n\tTRANSPORT_VISA\n',1)
texts['SCPIInstrument.cpp']=insert_once(texts['SCPIInstrument.cpp'],'\t\tcase SCPITransportType::TRANSPORT_VISA: return "visa";','\t\tcase SCPITransportType::TRANSPORT_LAN: return "lan";')
changes={root/n:t.encode('utf-8') for n,t in texts.items()}
for name in cpp_names+[n.replace('.cpp','.h') for n in cpp_names]+['Tek2221Status.h','Tek2221Resource.h']:
    target=root/name
    data=(source/name).read_bytes()
    if target.exists() and target.read_bytes()!=data:
        expected={old_hashes.get(name), v02_hashes.get(name), v03_hashes.get(name), v04_hashes.get(name), v041_hashes.get(name), v043_hashes.get(name), v044_hashes.get(name)}
        if hashlib.sha256(target.read_bytes()).hexdigest() not in expected:
            raise SystemExit(str(target)+' contains edits or another version; preserve/review them before replacing')
    changes[target]=data
# A header-only ImGui panel keeps the upstream GUI change small and explicit.
gui=a.checkout.resolve()/'src'/'ngscopeclient'
ui=gui/'StreamBrowserDialog.cpp'
if not ui.is_file(): raise SystemExit('Missing StreamBrowserDialog.cpp')
t=ui.read_text()
t=insert_once(t,'#include "Tek2221Panel.h"','#include "StreamBrowserDialog.h"')
t=insert_once(t,'\t\tif(scope) RenderTek2221Panel(scope);','\t\tvector<Oscilloscope::DigitalBank> digitalBanks;')
changes[ui]=t.encode('utf-8')
panel=gui/'Tek2221Panel.h'
paneldata=(Path(__file__).resolve().parent/'gui'/'Tek2221Panel.h').read_bytes()
if panel.exists() and panel.read_bytes()!=paneldata and hashlib.sha256(panel.read_bytes()).hexdigest() not in {"468df85353699bf5de7c885f1ee8b8ef80de7894f463d69d7cde0fa3acb90624", "a794fc32903fbc1e2dd466ea330e23f600ae40a09eb59525b27227e8d3da4f51"}:
    raise SystemExit(str(panel)+' contains an unrecognized earlier panel; preserve/review it before replacing')
changes[panel]=paneldata
# Recent-instrument history must preserve the opaque path after the first three colons.
menu=gui/'MainWindow_Menus.cpp'
if not menu.is_file(): raise SystemExit('Missing MainWindow_Menus.cpp')
menu_text=menu.read_text()
menu_text=insert_once(menu_text,'#include "Tek2221RecentConnection.h"','#include "MainWindow.h"')
old="auto fields = explode(cstring, ':');"
new="auto fields = SplitRecentInstrumentConnection(cstring);"
if new not in menu_text:
    if menu_text.count(old)!=1: raise SystemExit('Recent-instrument parser layout changed; review MainWindow_Menus.cpp')
    menu_text=menu_text.replace(old,new,1)
changes[menu]=menu_text.encode('utf-8')
history_header=gui/'Tek2221RecentConnection.h'
history_data=(Path(__file__).resolve().parent/'gui'/'Tek2221RecentConnection.h').read_bytes()
if history_header.exists() and history_header.read_bytes()!=history_data:
    raise SystemExit(str(history_header)+' contains unrecognized edits; preserve/review before replacing')
changes[history_header]=history_data
changes={f:data for f,data in changes.items() if not f.exists() or f.read_bytes()!=data}
if not changes:
    print('Native v0.4.4 driver is already applied.');raise SystemExit(0)
backup=root.parent/'.tek2221-backups'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
for f in changes:
    if f.exists():
        backup.mkdir(parents=True,exist_ok=True)
        shutil.copy2(f,backup/f.name)
for f,data in changes.items(): f.write_bytes(data)
print('Applied native 64-bit VISA v0.4.4. Previous files backed up in '+str(backup))
print('Rebuild ngscopeclient, then select tek2221 / native 64-bit VISA / GPIB0::5::INSTR.')
