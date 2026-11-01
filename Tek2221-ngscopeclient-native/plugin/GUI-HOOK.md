# Integrated Stream Browser controls (v0.6)

This build restores the previous Tek sidebar through a generic Windows GUI hook.
The host changes are one include, one call in StreamBrowserDialog.cpp, and one
new header. The host knows nothing about Tek controls or VISA. The DLL supplies
the panel and driver. A host rebuild is required once; subsequent Tek changes
can normally rebuild only the DLL, provided the host ABI/dependencies match.

Close ngscopeclient before replacing binaries. From UCRT64:

```sh
cd /c/Users/rccipriani/Downloads/Tek2221-ngscopeclient-native
python plugin/Apply-GUI-Hook.py /c/Users/rccipriani/source/scopehal-apps
cmake --build /c/Users/rccipriani/source/scopehal-apps/build -j4
bash plugin/Build-Plugin.sh /c/Users/rccipriani/source/scopehal-apps
bash plugin/Install-Plugin.sh /c/Users/rccipriani/source/scopehal-apps/build/src/ngscopeclient
bash plugin/Run-Stock.sh /c/Users/rccipriani/source/scopehal-apps/build/src/ngscopeclient
```

Use a fresh `tek2221_native` connection (transport `visa`, resource
`GPIB0::5::INSTR`). Expand the scope in Stream Browser and expand
**Tek 2221 controls (v0.4)**. That label identifies the restored panel layout.
The separate window is suppressed when the host advertises this hook.
Capture mode, delay, XY, probe factors, colored capture/action/query status,
acquisition settings, cursor/interface settings and one-shot actions are restored.
Rendering reads cached state; instrument operations remain asynchronous.

The hook uses exported `NgscopeclientRenderStreamPanelV1` functions from loaded
DLLs, with a versioned signature, current ImGui context and allocator callbacks.
The DLL compiles the exact host checkout's ImGui sources; it does not create a
second context or platform renderer. ImGui version and IO/style sizes are
checked before drawing. This is a prototype C++/ImGui ABI, not a universal
binary plugin SDK. Build both sides with matching dependencies and build type.
Release is the plugin default, matching the current host.

The installer is idempotent and retains StreamBrowserDialog.cpp.before-plugin-hook
on its first run. To remove the hook while retaining later edits, remove the
StreamBrowserPluginHook.h include and RenderStreamBrowserPluginPanels call,
then rebuild the host. Removing the Tek DLL leaves the hook harmless.
A stock host without the hook still uses the separate native controls window.

Acceptance test: expand panel, query one setting, change capture delay/probe
factor, link CH1/CH2, stop/resume, remove connection and exit. Runtime acceptance
requires Windows and the scope; local compilation alone does not verify it.
