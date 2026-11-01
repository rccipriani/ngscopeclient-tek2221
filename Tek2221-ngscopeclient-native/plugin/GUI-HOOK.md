# Optional Stream Browser GUI hook

**The hook is optional.** Without it, Tek2221Native.dll still captures waveforms
and opens its separate Windows controls window. With it, those controls render
under the scope in Stream Browser and the separate window is suppressed.
There is no second GUI DLL.

Follow the [main installation guide](../../README.md) for prerequisites and a
fresh build. To add the sidebar to an existing matching host, close ngscopeclient
and run from the source package directory in UCRT64:

```bash
python plugin/Apply-GUI-Hook.py /c/path/to/scopehal-apps
cmake --build /c/path/to/scopehal-apps/build -j4
bash plugin/Build-Plugin.sh /c/path/to/scopehal-apps
bash plugin/Install-Plugin.sh /c/path/to/scopehal-apps/build/src/ngscopeclient
bash plugin/Run-Stock.sh /c/path/to/scopehal-apps/build/src/ngscopeclient
```

Use driver `tek2221_native`, transport `visa`, and your GPIB resource. Expand the
instrument, then **Tek 2221 controls (v0.4)**. That retained layout label also
appears in v0.6. The sidebar includes capture mode/delay/XY, probe attenuation,
colored capture/action/query status, digital acquisition/cursor/interface
settings, and one-shot actions.

## Host changes

The script changes only:

1. One include in `src/ngscopeclient/StreamBrowserDialog.cpp`.
2. One call to `RenderStreamBrowserPluginPanels(scope)` in that file.
3. A new `src/ngscopeclient/StreamBrowserPluginHook.h`.

The hook is generic; the host has no Tek/VISA-specific implementation. The script
is idempotent and saves a first-run `.cpp.before-plugin-hook` backup. Unknown
source layouts are rejected rather than guessing where to insert the hook.
Do not run the old Apply-Driver.py as part of the DLL installation.

## Runtime contract

The host exports `NgscopeclientStreamPanelHookVersion`. On first rendering it
finds loaded modules exporting `NgscopeclientRenderStreamPanelV1`. The callback
receives a version, ImGui version/structure sizes, current ImGui context,
allocator callbacks and the current scope's shared_ptr. Plugins are loaded before
that renderer list is cached; runtime hot-loading/unloading is not supported.

The DLL compiles the exact host's ImGui sources and checks the version and
IO/style sizes before rendering. It uses the host's context, not a second frame
or graphics backend. Both sides still require matching C++/ImGui/toolchain ABIs;
this prototype is not a general binary compatibility promise. A rejected
renderer logs an error; it does not automatically reopen the native window when
the host still advertises the hook.

## Remove the hook

Close the app, remove the StreamBrowserPluginHook.h include and the
RenderStreamBrowserPluginPanels call from StreamBrowserDialog.cpp, remove the
new header, and rebuild the host. Preserve unrelated edits; use the backup only
if it still represents the desired file. Leave Tek2221Native.dll installed to
use its separate controls window.

Removing only the DLL leaves the generic hook harmless but removes this
project's Tek driver as well. Refer to the [validation checklist](../VALIDATION.md)
for capture, controls, session and shutdown checks.
