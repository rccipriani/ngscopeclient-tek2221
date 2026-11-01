## v0.6 Integrated sidebar

See [GUI-HOOK.md](GUI-HOOK.md) to install the generic host hook and restore the original sidebar. The plugin now defaults to Release to match the host Vulkan dispatcher layout.

## v0.5.1 Vulkan compatibility correction

The plugin now enables `VK_ENABLE_BETA_EXTENSIONS=1`, matching the host,
and uses the MSYS2 VulkanHeaders/VulkanLoader CMake targets. Omitting the
beta-extension definition changes the dispatcher layout and can cause a
`getVkHeaderVersion() == VK_HEADER_VERSION` assertion on first capture.
Assertions remain enabled. Build host and plugin against the same installed
Vulkan headers; if packages were updated since the host build, rebuild the host
before retesting. This release still needs a Windows capture test.

# v0.5 DLL prototype — no ngscopeclient source patches

The stock Windows `InitializePlugins()` scans the executable's `plugins` folder
and calls an exported `PluginInit()`. This prototype uses that entry point to
register **driver `tek2221_native`** and **transport `visa`**. It returns an empty
supported-model list, allowing the existing connection dialog to enumerate
registered transports without adding a VISA transport enum to the host.

**Do not run Apply-Driver.py for this approach.** Build-Plugin.sh configures an
external CMake project and reads the host headers/import libraries. It does not
copy driver files into scopehal or edit its CMake, enum, initialization or GUI
sources. Install copies only Tek2221Native.dll to the host's plugins folder.

The prototype is source-only. All seven plugin source files compiled to Windows
x86-64 object files against a pristine upstream header tree with no VISA enum.
Actual DLL linking, loading and hardware use need testing on Tachi against the
matching MSYS2 libraries. Do not assume compatibility with an arbitrary prebuilt
release or a host built using another compiler/runtime. C++ class layouts and
exported symbols must match: use the exact host checkout, build and UCRT64
compiler/dependencies. Runtime libscopehal/liblog remain the host's originals.

## How the UI differs

CH1, CH2, REF4 and Signal/Max + Min remain in stock Stream Browser and work with
normal plots, Run/Single/Stop and probe-factor controls. There is **no custom Tek
controls node inside Stream Browser** in the unmodified host.

A separate native Windows **Tek2221 controls** window opens when the plugin
connects. It offers:

- Paired/XY capture, delay and per-physical-channel probe factors. Click **Apply
  local settings** to commit those fields, or **Read local settings** to reload
  them from the driver (e.g. after reopening an ngscopeclient session).
- A scope-control selector plus explicit Read and Apply value buttons. Enum
  values currently use the documented command abbreviations: SAM=Sample,
  PEA=Peak Detect, AVE=Average, ACC=AccPeak. Numeric controls accept typed values;
  the backend still validates all values and verifies readback.
- An action selector and Execute action button, with setup-slot, CRT-message-row
  and message inputs. Save/erase reference actions overwrite scope memory.
- Separate capture/action/query outputs. These are read-only multiline native
  text boxes: focus one and use Ctrl+A/C to copy. Output colors are not ported
  from the custom ImGui panel in this prototype.

The window's timer reads cached driver state only; it does not poll the scope.
Closing the window hides it; its tray icon reopens it. Removing the instrument
closes its window/thread before releasing the driver/VISA session. This is a UI
within the same DLL/process, not a bridge, second instrument connection or
external capture service. Local settings/setup slots still use ngscopeclient
session serialization. Current status is not restored as historical confirmation.

The plugin also normalizes `GPIB0:5:INSTR` back to `GPIB0::5::INSTR` on input,
working around the stock recent-menu delimiter bug without patching that menu.
This repair is restricted to valid GPIB resources; it does not reinterpret LAN
or arbitrary strings as instrument addresses.

## Fast initial load test with the existing matching build

This verifies external compilation/loading using the build already on Tachi.
That host still has the previous patches; this initial test alone does **not**
prove a clean host. Use the separate stock build below for that final check.
Close all scope sessions and ngscopeclient first. From UCRT64:

```sh
cd /c/Users/rccipriani/Downloads/Tek2221-ngscopeclient-native
bash plugin/Build-Plugin.sh /c/Users/rccipriani/source/scopehal-apps
bash plugin/Install-Plugin.sh /c/Users/rccipriani/source/scopehal-apps/build/src/ngscopeclient
bash plugin/Run-Stock.sh /c/Users/rccipriani/source/scopehal-apps/build/src/ngscopeclient
```

Expected startup log:

```
Tek2221 native VISA DLL plugin loaded (driver tek2221_native, transport visa).
```

Add → Oscilloscope → Connect…:

| Field | Value |
| --- | --- |
| Nickname | Tek2221 DLL |
| Driver | `tek2221_native` |
| Model | No fixed model list (expected) |
| Transport | `visa` |
| Path | `GPIB0::5::INSTR` |

Use a fresh connection, not a saved session referring to the old `tek2221`
driver. If testing beside the patched host, use **only `tek2221_native`** for new
connections: the plugin registers its VISA factory under the same `visa` name.
Implementation class names are distinct from the earlier built-in driver to
avoid accidental binding to its C++ class symbols. Do not open a patched-driver
connection simultaneously or run WaveStar/NI-MAX test sessions against the scope.

First check plugin startup/driver selection, then ID and CH1 Single. Verify the
separate window appears; follow with CH2, paired capture and Save/Restore. Close
and reopen its window through the tray icon, and remove/re-add the instrument.
Test recent reconnection without manually restoring colons. Only then test on
an unmodified host.

Run-Stock.sh changes the working directory to the EXE directory: the current
stock Windows loader finds the plugins directory by EXE path, but loads each
DLL using a relative `plugins` path. This launch wrapper accommodates both
without altering the loader. Use the wrapper or an equivalent shortcut with
"Start in" set to the EXE directory. Run it from UCRT64 so dependency DLLs from
ucrt64/bin are on PATH. Build/install scripts leave original host DLLs intact.

## Build a separate, unmodified host

Keep the working patched checkout. Create a second checkout at the known source
baseline, reusing the already installed UCRT64 dependencies. Git checkout and
normal generated build files are expected; **none of our patches are applied**:

```sh
cd /c/Users/rccipriani/Downloads/Tek2221-ngscopeclient-native
mkdir -p /c/Users/rccipriani/AppData/Local/Temp/msys2
export TMPDIR=/c/Users/rccipriani/AppData/Local/Temp/msys2
export TMP="$TMPDIR"
export TEMP="$TMPDIR"
cd /c/Users/rccipriani/source
git clone --recursive https://github.com/ngscopeclient/scopehal-apps.git scopehal-apps-stock
cd scopehal-apps-stock
git checkout aa019065096a8c258b8c91a18453bcf7e0566274
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF \
  -DCMAKE_SHARED_LINKER_FLAGS=-fopenmp -DCMAKE_EXE_LINKER_FLAGS=-fopenmp \
  '-DCMAKE_RC_FLAGS=--preprocessor=gcc --preprocessor-arg=-E --preprocessor-arg=-xc-header --preprocessor-arg=-DRC_INVOKED'
cmake --build build -j4
cd /c/Users/rccipriani/Downloads/Tek2221-ngscopeclient-native
bash plugin/Build-Plugin.sh /c/Users/rccipriani/source/scopehal-apps-stock
bash plugin/Install-Plugin.sh /c/Users/rccipriani/source/scopehal-apps-stock/build/src/ngscopeclient
bash plugin/Run-Stock.sh /c/Users/rccipriani/source/scopehal-apps-stock/build/src/ngscopeclient
```

The dependency/toolchain build flags above are normal build configuration, not
source patches. The pinned libscopehal baseline is
`d7461694340a4b18093b843163ebc5f776780bb0`. After plugin build/install, `git diff`
in the stock app and lib trees should show no tracked source changes.

A host/compiler/dependency upgrade may require rebuilding this DLL. There is
no stable binary plugin ABI guarantee established by this prototype. No patch
to the host or NI VISA DLL and no NI SDK import library is used; System32
visa64.dll is loaded as in the validated native driver.

## Rollback and diagnostics

Close the host and remove/rename `plugins/Tek2221Native.dll`. The installer saves
a previous plugin as `Tek2221Native.dll.bak`; it never replaces host DLLs. Launch
the existing patched build to return to the validated v0.4.4 path.

If linking fails, send the linker output. If the plugin fails to appear, send
the terminal startup log and check the EXE working directory and dependency PATH.
A successful object compilation does not establish that the host exports every
needed C++ symbol. Export/link failures and runtime load behavior are the next
on-machine acceptance gates. Hardware/disconnect/UI lifetime tests remain needed.
The prototype's native window is Windows-only and intentionally simpler than
our patched ImGui panel.
