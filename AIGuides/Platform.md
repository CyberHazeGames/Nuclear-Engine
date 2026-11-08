# Platform foundation

[Nuclear.Platform](../Nuclear.Platform/Nuclear.Platform.vcxproj) is an independent C++20 DLL below Nuclear.Engine. It replaces SDL and owns native windows, event translation, keyboard/mouse state, cursor modes, clipboard text, executable discovery, and dynamic library loading. It has no dependency on Nuclear.Engine, Diligent, ImGui, or engine logging.

Only **Windows x64** is implemented. Public headers under [Nuclear.Platform/include/Nuclear.Platform](../Nuclear.Platform/include/Nuclear.Platform) use standard C++ types and opaque native handles. Windows APIs and message translation live under [Source/Windows](../Nuclear.Platform/Source/Windows). Additional platforms require their own implementation and build selection; these boundaries do not establish that the rest of the engine is portable.

[Networking](Networking.md) adds independent UDP/TCP sockets, IPv4/IPv6 endpoints, synchronous hostname resolution, readiness waits, and explicit operation results. Sockets initialize and retain the networking runtime automatically, work without a window/event thread, and default to nonblocking mode. Higher-level replication and session protocols belong above the foundation.

[Diagnostics](Profiling.md#platform-cpu-gpu-and-memory-queries) exposes CPU topology/counters, process/system memory, GPU adapter identities, and exact-adapter process memory budgets without a window. Native Windows/DXGI types stay private. Renderer query objects, allocation accounting, capture history, and the ImGui profiler stay in Nuclear.Engine.

## Lifetime and event flow

`WindowDesc::CustomTitleBar` opts a non-fullscreen window into client-drawn chrome.
It retains the native overlapped-window capabilities but removes the nonclient
caption via `WM_NCCALCSIZE`. The backend supplies DPI-aware resize hit tests and
maximized bounds matching the current monitor's work area. `SetTitleBarHitTest`
accepts a callback in physical client pixels; true exposes `HTCAPTION` for native
dragging and double-click/system-menu behavior. UI owners must clear callbacks
before captured widgets expire. `Destroy` also clears the callback.
`Minimize`, `ToggleMaximize`, `IsMaximized` and `RequestClose` keep OS commands
inside the foundation. RequestClose posts the existing deferred close event.
Defaults and fullscreen behavior remain unchanged; public headers expose no OS types.

1. Call `Nuclear::Platform::Initialize()` on the thread that will own windows and pump events.
2. Create a `Platform::Window` with `WindowDesc`. Dimensions are client pixels and titles are UTF-8. `FullScreen` creates a borderless window over the primary monitor; it does not change the display mode.
3. Pass the borrowed window to `GraphicsModuleDesc::pWindowHandle`. Graphics converts `GetNativeHandle()` to Diligent's native-window description at the renderer boundary.
4. Drain `Platform::PollEvent(Event&)` before each frame. It pumps Windows messages and returns portable events. Physical key identifiers retain the previous USB HID values, including the values in `Nuclear.ScriptCore/Platform/Input.cs`. Text input is separate Unicode scalar data, with UTF-16 surrogate pairs combined by the Windows backend.
5. Forward events to [ImGuiPlatform](../Nuclear.Engine/Source/Graphics/ImGuiPlatform.cpp) and dispatch close, resize, movement, and scroll callbacks to the client. Held keys/buttons are cleared on focus loss. Locked mouse mode uses foreground raw relative motion, hides the cursor, and confines it to the focused client area. Confinement is released on focus loss and restored when focus returns.
   The engine also routes events through [retained UI](UI.md) before gameplay mouse
   callbacks. MouseButton events contain the button message's client coordinates
   (virtual coordinates in locked mode), including clicks without prior MouseMove.
   The foundation remains independent of both retained UI and ImGui.
6. Shut down clients and modules, detach ImGui, and destroy graphics resources before destroying the window. A close event leaves its native handle alive for this cleanup. Finally call `Platform::Shutdown()`.

The platform API is intended for the window/event thread. Window objects cannot be copied. Native handles, queued event window pointers, and library symbol addresses are borrowed and must not outlive their owners. Destroying a window removes its queued events. Repeated `Destroy()` and `Shutdown()` calls are safe; shutdown refuses to unregister the backend while windows remain alive.

`Window::ClientToScreen` and `ScreenToClient` convert signed pixel coordinates
through the desktop for cross-window drag/drop. They return false for an empty
window without modifying coordinates. Secondary retained editor windows use these
portable helpers; native handles remain at the foundation/graphics boundaries.
The engine forwards secondary events to `Client::OnAuxiliaryWindowEvent` rather
than sending them through the main UI context or closing the engine. Clients own
secondary-window close/resize/input behavior and release their swap chains before
destroying the corresponding platform windows. This is separate from ImGui native
viewports, which remain unsupported by the legacy backend.

## Engine boundaries and compatibility

- Existing engine includes `Platform/Window.h` and `Platform/Input.h` forward to the foundation headers. Callers must add `Nuclear.Platform/include` to their include directories and link `Nuclear.Platform.lib`; the engine, samples, and editor projects reference the foundation project directly. Ship `Nuclear.Platform.dll` beside their executables.
- `Window::GetSDLWindowPtr()` is removed. Use `GetNativeHandle()` only at an actual native integration boundary. `GraphicsModuleDesc::pWindowHandle` now accepts `Platform::Window*`, rather than an SDL pointer.
- Engine `Platform::FileSystem` remains in Nuclear.Engine because its API depends on `Core::Path`, `Utilities::DataBlob`, logging, and shader processing. It uses the standard filesystem library and is not a low-level native window service.
- Module and audio plugin discovery, ABI validation, object ownership, and teardown stay in their engine subsystems. Native library load/symbol/unload operations and executable path lookup go through the foundation. Windows library loading uses an absolute path and searches that library's directory plus the default DLL directories.
- [ImGuiPlatform](../Nuclear.Engine/include/Graphics/ImGuiPlatform.h) translates portable events into ImGui input, handles UTF-8 clipboard callbacks, cursor shapes, display dimensions, and timing. The existing Diligent renderer is retained. Docking inside the main window is supported; secondary native ImGui viewports, gamepads, and explicit IME composition positioning are not implemented by the new backend. It does not advertise those capabilities.

## Extending to another platform

Implement the existing public contracts in a new platform source directory and select it in a corresponding native build configuration. Keep native headers out of the public API, preserve key values and mouse masks, and provide event/focus/clipboard/cursor behavior consistently. Add a Diligent native-window adapter in the graphics integration for the new platform. Do not place engine dependencies in the foundation to make a port compile.

## Validation

The normal Debug solution build includes Nuclear.Platform. The independent [Windows smoke project](../Nuclear.Platform/Nuclear.Platform.Tests.vcxproj) is deliberately separate from the solution's default build. Build it in a Visual Studio developer shell with the normalized process environment described in [BuildAndValidation.md](BuildAndValidation.md):

```powershell
msbuild Nuclear.Platform/Nuclear.Platform.Tests.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
./Bin/DebugX64/Nuclear.Platform.Tests.exe
```

The test briefly creates a native window and checks client dimensions/resizing, UTF-8 titles, close-event lifetime, physical and extended keys, focus clearing, Unicode input, wheel translation, invalid inputs, executable discovery, dynamic library lookup, and repeated cleanup/reinitialization. Synthetic Windows messages verify translation; they do not validate physical mouse devices, camera feel, clipboard interaction, or audio playback. Exercise those in a sample when changing the corresponding behavior.

The separate [engine smoke project](../Nuclear.Platform/Nuclear.Platform.EngineSmoke.vcxproj) exercises the engine, Diligent swap chain, and ImGui integration without requiring audio, physics, scripting, or worker threads. Build it with the same MSBuild options, then run it from `Samples/` so engine assets resolve:

```powershell
msbuild Nuclear.Platform/Nuclear.Platform.EngineSmoke.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
Push-Location Samples
try { & ../Bin/DebugX64/Nuclear.Platform.EngineSmoke.exe }
finally { Pop-Location }
```

Migration validation on 2026-11-06: the Debug x64 solution built, the standalone Windows smoke checks passed in Debug and Release, and the Debug engine smoke rendered 12 frames, resized to 960x540, received mouse/scroll callbacks, and completed client/module/window shutdown with exit code 0. Binary dependency inspection confirmed that Nuclear.Engine imports Nuclear.Platform and no SDL DLL. Nuclear.Platform imports only Windows system and C++ runtime DLLs. The default Samples launch initialized graphics/ImGui but failed XAudio master-voice creation, preventing successful full startup. A full Release engine build, physical input feel, clipboard interaction, and secondary-window UI behavior were not validated.

Custom chrome checks on 2026-11-08 passed in Debug and Release: requested client
sizes, caption/client/corner hit tests, native caption double-click maximize/restore,
client double-click press translation, work-area maximize bounds, restore dimensions,
minimize, deferred close and callback cleanup. The Debug editor integration and
hidden executable launch also passed; see [Editor.md](Editor.md#launch-and-validation).
Physical input feel, automatic DPI transitions and a full Release engine remain unverified.
