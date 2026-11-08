# Build and validation

## What the repository currently specifies

Use [Nuclear Engine.sln](../Nuclear%20Engine.sln) as the concrete Windows build entry point. It includes the Nuclear.Platform foundation DLL, the engine DLL, the C# scripting static library, the buildable Lua scaffold static library, XAudio2 and OpenAL audio DLL projects, Samples, editor, ScriptCore, SamplesScripts, and Nuclear.Managed. Engine native projects use C++20 and Debug/Release x64 configurations; Nuclear.Managed.Native uses C++17. Managed projects target .NET 10 and map to Any CPU in the solution; the native host and installed runtime must be x64. Nuclear.Engine links the C# backend through a project reference; the Lua scaffold is not linked or registered. Audio DLLs link against the engine import library and are loaded by name at runtime.

Toolset details are not uniform:

- [RunCmakeForDependencies.bat](../RunCmakeForDependencies.bat) selects `Visual Studio 18 2026` and x64 for Assimp, Diligent, and OpenAL. Diligent and OpenAL also receive `CMAKE_POLICY_VERSION_MINIMUM=3.5`.
- The solution and first-party native projects (platform, engine, editor, samples, scripting plugin libraries, audio plugin DLLs, and Nuclear.Managed.Native) target Visual Studio 18 / **v145** for x64 Debug and Release. The installed MSVC toolset inspected on 2026-09-23 was 14.51.36231.
- Dependency copy paths and `#pragma comment(lib, ...)` directives include older toolset-specific names such as `assimp-vc143-mt` and PhysX `vc142` output directories.

Inspect installed toolsets and generated filenames before deciding whether to adjust anything. A generator update alone does not update import-library names, dependency output paths, or all native projects.

## Dependency preparation

The repository does not provide a single complete bootstrap command. For setup work:

1. Inspect `git submodule status` and `.gitmodules`; preserve local submodule changes. If initialization is needed for the task, use `git submodule update --init --recursive` without `--remote` to obtain recorded revisions.
2. Read the dependency batch scripts before running them. They use repository-relative paths and interactive `pause` statements.
3. Configure Assimp, Diligent, and OpenAL using the commands in `RunCmakeForDependencies.bat`, then build the required configurations. Configuration alone does not compile the libraries.
4. Supply/build the remaining dependencies: PhysX, FreeImage, FreeType, msdf-atlas-gen/msdfgen, and libsndfile. SDL headers, import libraries, and runtime DLLs are no longer needed; Nuclear.Platform builds from first-party source. For scripting, install the x64 .NET 10 runtime and .NET 10 SDK. Nuclear.Managed is included as source; no scripting submodule is required. Inspect project include paths and native library pragmas for the actual expected layout.
5. Review [CopyDependenciesBinaries.bat](../CopyDependenciesBinaries.bat) against the outputs that were actually produced. It copies a subset of prerequisites and always exits with code 0; a successful exit does not prove every copy succeeded.

Example dependency build commands, **after successful configuration**:

```powershell
cmake --build External/CmakeProjects/Assimp --config Release --parallel
cmake --build External/CmakeProjects/Diligent --config Debug --parallel
cmake --build External/CmakeProjects/OpenAL --config Debug --parallel
```

Build Diligent/OpenAL Release when targeting an engine Release build. The copy script uses Release Assimp and FreeImage binaries for both engine configurations. These commands reflect the scripts' layout; they were not executed during this documentation task.

## Native and managed build

From the repository root, with Visual Studio and the required .NET SDK installed:

```powershell
./BuildSupport/Build.ps1 -Configuration Debug
```

The [build runner](../BuildSupport/Build.ps1) locates Visual Studio's 64-bit MSBuild, normalizes duplicate `Path`/`PATH` entries in the process environment, restores managed dependencies, and builds the full solution with one worker. Duplicate environment entries caused compiler task exceptions, and parallel MSBuild exited without a diagnostic in the inspected launch environment. The runner does not change the machine's environment settings. Pass `-Configuration Release` to select Release; the successful full build recorded below covers Debug only.

For an engine-only change, a narrower compile/link check in a Visual Studio developer shell with a normalized process environment is:

```powershell
msbuild Nuclear.Engine/Nuclear.Engine.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
```

The engine resolves native library directories relative to its project, so standalone builds do not require `SolutionDir`. Diligent GraphicsTools also requires `Diligent-GraphicsEngineD3DBase.lib`, `Diligent-GraphicsEngineOpenGL-static.lib`, and `Diligent-GraphicsEngineVk-static.lib`; the engine links them and the dependency copy script includes them for both configurations.

The solution build includes managed project dependencies and their post-build copying into `Samples/`. Native and managed build outputs go to `Bin/DebugX64/` or `Bin/ReleaseX64/`; Nuclear.Platform.dll and both audio DLLs are beside the engine and sample executables. Engine and audio plugin intermediates are under project-local `Build/` folders; Nuclear.Managed.Native intermediates are under the root `Build/`.

### Module manager and audio plugins

`Engine::GetModuleManager()` accepts owned `EngineModule` instances, references to longer-lived singleton modules, and external DLL modules before `Start`. Built-in modules register directly and initialize in dependency order; only the asset manager uses a callback adapter. The manager rejects missing dependencies and cycles, and reverses completed phases on failure or shutdown. `AssetLibrary` path setup and Nuclear.Platform/window creation remain outside the manager.

To load one external module DLL before startup, call `Engine::Get().GetModuleManager().LoadPlugin(path)`. To scan a directory at engine startup, set `EngineStartupDesc.mModulePluginDirectory`; `LoadPlugins(directory)` loads `Nuclear.Module.*.dll` files in sorted order. Each DLL exports `GetNuclearEngineModulePlugin` with the size/version and factory table in [EngineModulePlugin.h](../Nuclear.Engine/include/Core/EngineModulePlugin.h). A factory can declare dependencies on built-in or other DLL modules. The DLL and engine must use compatible C++ ABI, architecture, and runtime configuration. The manager calls plugin destroy functions before `FreeLibrary`; loading while modules run and hot reload are not supported.

Set `EngineStartupDesc.mAudioBackendName` to `"XAudio2"` (default) or `"OpenAL"`. `AudioModule` loads `Nuclear.Audio.<name>.dll` from the executable directory, or from `EngineStartupDesc.mAudioPluginDirectory` when supplied. It validates the exported plugin ABI, initializes the selected backend, and unloads the DLL during shutdown. Both audio projects must be built for the same configuration as the engine. Backend initialization failure now fails engine startup; a usable audio device is needed when audio auto initialization is enabled.

After the external module DLL loader was added on 2026-09-25, the full `BuildSupport/Build.ps1 -Configuration Debug` solution build passed. Engine startup with `mModulePluginDirectory` has not been launched in a graphical sample.

### .NET scripting dependencies and checks

`Nuclear.Managed` is an included source fork of Coral, with upstream revision and MIT attribution recorded in its [README](../Nuclear.Managed/README.md). The [native project](../Nuclear.Managed.Native/Nuclear.Managed.Native.vcxproj) builds `Nuclear.Managed.Native.lib`; the [managed project](../Nuclear.Managed/Nuclear.Managed.csproj) builds `Nuclear.Managed.dll` for `net10.0`. No Coral submodule or Premake step is required. The host parses complete numeric versions and selects the highest stable .NET 10 hostfxr in an installation directory. Windows uses the system installation under Program Files; custom `DOTNET_ROOT` discovery is not implemented.

```powershell
dotnet build SamplesScripts/SamplesScripts.csproj -c Debug /m:1 /nr:false
```

ScriptCore's build copies `Nuclear.Managed.dll`, its runtime configuration/dependency manifests, and ScriptCore's runtime files into `Samples/`; SamplesScripts copies its own assembly and dependency manifest there. No separate runtime-directory discovery or legacy .NET Framework targeting pack is required.

After the module manager and audio plugin split on 2026-09-25, `BuildSupport/Build.ps1 -Configuration Debug` built the full solution, including both scripting libraries and both audio DLLs. A graphical sample launch and audio-device initialization were not part of that build. A full Release solution build has not been verified.

The root CMake setup is unfinished: it contains `set(CMAKE_MODULE_PATH "${/cmake")`, sparse dependency wiring, and a sample entry `main.cpp` while the actual file is `Main.cpp`. Do not document `cmake -S . -B ...` as a verified alternative without explicitly repairing and validating that build path.

## Running samples

[Samples/Main.cpp](../Samples/Main.cpp) expects `../Assets/` and loads `SamplesScripts.dll` from the current working directory. `Engine::Start` also looks for `Nuclear.ScriptCore.dll`, `Nuclear.Managed.dll`, and `Nuclear.Managed.runtimeconfig.json` in that directory. Nuclear.Managed hosts the installed x64 .NET 10 runtime.

The intended sample working directory inferred from those paths and the managed post-build events is `Samples/`:

```powershell
Push-Location Samples
try {
    & ../Bin/DebugX64/Samples.exe
}
finally {
    Pop-Location
}
```

Before launch, verify the managed DLLs and runtime manifests, native runtime DLLs beside the executable, the installed .NET runtime, and the content needed by the chosen sample. Startup defaults to D3D11; `SelectRenderer()` exists in `Main.cpp` but is not called by the active startup code.

After the Nuclear.Managed migration on 2026-09-23, Debug Samples launched from `Samples/`, initialized the renamed scripting bridge and both script assemblies, and reached the selector's rendering state. Loaded-module inspection confirmed `hostfxr.dll` and `coreclr.dll` from .NET 10.0.12. Old Coral runtime files were absent from the runtime directories. XAudio reported a master-voice creation failure, while engine initialization continued. This was a startup check, not a Sample3 interaction or graceful-shutdown check; the validation process was stopped afterward.

Before the Nuclear.Managed/.NET 10 migration, launch validation on 2026-09-23: the Debug executable launched with `Samples/` as its working directory, initialized D3D11 on an NVIDIA GeForce RTX 5070 Ti and Coral/.NET scripting, entered the selector's rendering loop, and remained running. The log subsequently recorded Sample3's managed `OnStart`. Sample3 reported missing imported PBR shader/material/texture assets and postprocessing render-target format warnings; this is not validation of every sample's content or rendering. Native UI inspection was unavailable, so confirmation was through process state and the startup log.

The fallback module now creates its black, grey, white, and flat-normal textures synchronously in memory; the formerly required `DefaultTextures/*.png` files are no longer startup prerequisites. Synchronous texture imports check decoding before creating GPU data and return the black fallback on failure.

A subsequent selector launch also reached rendering, then exited with `0xC000013A` (`STATUS_CONTROL_C_EXIT`, console cancellation). Do not mistake that exit code for the earlier missing-texture startup failure.

Logs are written to `NuclearEngine.log` in the working directory and truncated when the logger is constructed. Save a relevant failure log before rerunning if it is needed for comparison.

The Playground audio test creates its listener/source on the controller through `AudioSystem`, after a successful clip import with a nonzero buffer. `AudioSourceComponent::SetAudioClip` only assigns/binds a borrowed clip; it does not create a backend source. Controls require a nonzero source handle and a loaded/created clip with a nonzero buffer, and pitch uses the backend pitch operation. `SetIsLooping` only caches the component flag; backend looping still comes from the clip at source creation. Missing Playground audio content skips playback, and shutdown pauses playback, restores normal mouse input, and clears the scene before flushing imported assets.

On 2026-11-06, a temporary Debug x64 harness reproduced the Playground crash as a vector bounds assertion: the standalone component passed source ID zero to XAudio2 before source creation. After the fix, the full `BuildSupport/Build.ps1 -Configuration Debug` solution build passed. The harness loaded the actual Playground twice and rendered 240 D3D11 frames per run, both with the local `str3.mp3` and with its asset path deliberately missing. It checked source/buffer creation, exercised volume/pitch and pause/play calls, tested controls on empty/uncreated sources, and verified scene/system clearing; both runs exited with code 0. The missing `arial.ttf` remained nonfatal. Shutdown logged a PhysX pending-module-reference error despite the zero exit status. This verifies the automated Playground lifecycle and control calls, not manual selector interaction, audible playback, a Release build, or other rendering/audio backends.

### LearnOpenGL sample content

The samples resolve `@CommonAssets@` to `Assets/Common/` when launched from `Samples/`. For content supplied by a LearnOpenGL download, place these files from its `resources/` directory at the following destinations under `Assets/Common/`:

| LearnOpenGL source | Sample destination |
| --- | --- |
| `objects/nanosuit/` OBJ, MTL, and referenced textures | `Models/CrytekNanosuit/` |
| `objects/cyborg/` OBJ, MTL, referenced textures, and license | `Models/CrytekCyborg/` |
| `objects/vampire/dancing_vampire.dae` and `textures/` | `Models/vampire/`, preserving the texture subdirectory |
| `textures/pbr/rusted_iron/` | `Textures/PBR/RustedIron/` |
| `textures/pbr/` folders `plastic`, `grass`, `gold`, `wall` | Corresponding folders under `Textures/PBR/` |
| `textures/skybox/` six JPG faces | `Skybox/` |
| `textures/hdr/newport_loft.hdr` | `Textures/HDR/newport_loft.hdr` |
| `textures/container2.png`, `textures/container2_specular.png` | `Textures/crate_diffuse.png`, `Textures/crate_specular.png` |

Sample1 and the selector import the supplied vampire Collada model as `dancing_vampire.dae`. This content set does not supply Bob, CrytekSponza, shaderball, `crate_normal.png`, `arial.ttf`, or the requested `str3.mp3`; provide those separately where used. Moving source content does not generate the imported assets required by Samples 2–5. The selector's **Build Assets Library** action performs imports and also needs its other model inputs. Most source content is ignored by Git; its presence locally does not make a clean checkout complete.

## Retained editor validation

Undo/redo follow-up on 2026-11-08: Debug x64 builds of the editor, focused editor
tests and UI tests passed. Run `../Bin/DebugX64/Nuclear.Editor.Tests.exe` from
`Nuclear.Editor/` for history ordering, restored entity identities/component data,
menu/keyboard/gizmo transactions, redo branching/no-ops/retention, detached-window
shortcuts, modal/text isolation, scene/project resets and the existing D3D11
docking/import/readback/teardown suite. Headless UI checks passed, including
preview-event consumption, focus/input-mode gating and callback cleanup.
`../Bin/DebugX64/Nuclear.UI.Tests.exe --gpu --imgui` from `Samples/` passed its
60-frame D3D11 lifecycle/readback checks. This follow-up did not run a full
solution/Release build, other graphics backends or manual input testing.

On 2026-11-08, the full Debug x64 solution build passed after replacing the editor
with retained UI. The separate `Nuclear.Editor/Nuclear.Editor.Tests.vcxproj` build
and executable passed D3D11 framebuffer checks at 1440x900 and 960x600, toolbar
creation/deletion, entity rename and numeric position editing, component addition,
modal focus isolation/cancel/create, capture/history, local OBJ geometry import and
instantiation, and workspace teardown/reload. Scene pixel coverage was checked
separately from UI glyph coverage; both readbacks were visually reviewed.

Debug headless UI tests passed including Unicode editing/deletion, focused-content
replacement and scrolling/focus-reveal regressions. `Nuclear.UI.Tests.exe --gpu
--imgui` passed its existing 60-frame D3D11 readback/lifecycle/native-button checks.
The actual `Nuclear.Editor.exe`, launched from `Nuclear.Editor/`, reached its
rendering loop and shut down normally with exit code 0 after a window-close
message. That launch used a hidden window. See [Editor.md](Editor.md) for startup
configuration, the test project and session-authoring limitations.

The subsequent editor gizmo checks passed in Debug: all world axes and operations,
free move/uniform and mirrored scale, edge-on rotation, invalid/hidden pivots,
scaled input, Escape/focus/Tab/resize cancellation, deleted targets, and live
inspector synchronization. D3D11 checks additionally exercised drag commit outside
the viewport and right-button camera look. Move, rotation and scale framebuffer
readbacks were visually reviewed. These checks use the existing editor test project;
no engine dependency or backend was added for gizmos.

These checks do not validate a full Release engine build, other rendering backends,
physical mouse/keyboard feel, clipboard interoperability, arbitrary model formats,
material/animation importing, runtime physics/audio/scripts, or disk project
persistence. The build retains existing dependency PDB/runtime-library warnings;
shader compilation reports existing loop-unroll warnings.

## Choosing validation

The document-workspace isolation follow-up on 2026-11-08 passed Debug editor and
focused editor-test builds. The D3D11 integration run checks asset viewers without
viewport panels, preserved per-document docking/control state, custom-tool ownership
and scoped Window/reset actions, whole-document native transfer, and typed fallback
titles for empty/whitespace texture/mesh/material names. Existing viewport/gizmo,
import, modal and teardown/reload checks also passed. These focused checks do not
establish other graphics backends or a Release engine build.

The editor docking change on 2026-11-08 passed the full Debug x64 solution build,
the focused editor tests, headless UI tests, the 60-frame D3D11 UI test with ImGui,
and Windows platform smoke tests in Debug and Release. The editor tests exercise
asset documents, tab reuse/close/reorder, state-preserving docking, splitters,
native-window rendering/input/resize, cross-window redocking, window close,
panel recovery/reset, modal isolation and teardown with a floating panel open.
The actual Debug editor also reached rendering and exited normally after a window
close message in a hidden launch. See [Editor.md](Editor.md#document-tabs-and-docking)
for use and ownership. Secondary D3D12/Vulkan swap-chain paths are source-level
implementations only; OpenGL tear-off is unavailable. Full Release engine builds,
physical drag feel, automatic DPI and saved layouts were not validated.

On 2026-11-08, the UI class-per-file reorganization passed the full Debug x64
solution build and independent compilation of all 26 public UI headers. Existing
headless UI tests, both D3D11 UI GPU modes, and both retained-selector modes passed.
The sample and both focused test projects now compile `UISample.cpp` and
`UIShowcaseWidget.cpp` explicitly. This refactor did not validate a full Release
engine build or other rendering backends.

The retained sample-selector migration was validated on 2026-11-07 with the full
Debug x64 solution build and the separate `Samples/Samples.SelectorTests.vcxproj`
project. Run `Samples.SelectorTests.exe` from `Samples/`, with optional `--imgui`.
Both D3D11 modes passed VSync/widget and capture toggles, expanded-panel keyboard
reachability, scaling from 1280x720 to 960x540, native keyboard/mouse launching of
Runtime UI, clear-history, queued asset-import cancellation, sample return, and
fresh-selector reload. Readback images were visually
reviewed. The test disables audio/physics/scripting/threading/3D scene rendering
and does not import sample libraries or launch the other content-dependent samples.

Runtime UI has a separate `Nuclear.Engine/Nuclear.UI.Tests.vcxproj` project with
headless layout/input tests and optional D3D11 readback checks. See [UI.md](UI.md#validation-and-launch)
for commands. The interactive `Samples.exe --ui` path runs retained widgets with
ImGui, audio, physics, scripting, worker threads, and 3D scene rendering disabled.

On 2026-11-07, the full Debug x64 solution built with retained UI. Debug headless
UI checks passed; D3D11 GPU checks passed with ImGui disabled and enabled (two
30-frame clients per mode). Readback checked alpha blending, painter order,
clipping, and SDF text; lifecycle checks covered resize, minimize/restore, scaling,
font-library clearing, client reload, and shutdown. Synthetic platform mouse events
activated retained controls through engine routing. Platform smoke tests passed in
Debug and Release after adding mouse-button coordinates. Other UI rendering
backends and a full Release engine build were not validated.
The standalone `Samples.exe --ui` entry point reached rendering and completed
graceful shutdown with exit code 0 after a normal engine-window close message;
this automated launch used a hidden window.

Profiling adds the Windows SDK's `dxgi.lib`/`psapi.lib` to Nuclear.Platform and optional Diligent GPU query features. See [Profiling.md](Profiling.md#validation) for separate platform diagnostics, CPU/accounting, and GPU test projects. On 2026-11-06 the diagnostics executable passed in Debug and Release; CPU/accounting tests and the 70-frame D3D11 GPU/UI test passed in Debug, and the full Debug solution built. This does not validate a full Release engine build or GPU query behavior on the other rendering backends.

The [networking test project](../Nuclear.Platform/Nuclear.Platform.NetworkTests.vcxproj) builds a headless loopback executable separately from the solution's default projects. See [Networking.md](Networking.md#validation) for Debug/Release build/run commands. Nuclear.Platform links the Windows SDK's `ws2_32.lib`; no additional third-party networking SDK or runtime distribution is required.

On 2026-11-06, the networking loopback tests passed in Debug and Release for IPv4/IPv6 UDP/TCP, and the full Debug solution build passed with the new API. These checks do not validate replication, remote networks, or a full Release engine build.

After the Nuclear.Platform migration on 2026-11-06, the full Debug x64 solution build passed. The standalone Windows platform smoke project passed in Debug and Release. The separate Debug engine smoke rendered 12 frames through Diligent/ImGui, resized to 960x540, received mouse/scroll callbacks, and shut down with exit code 0, with audio/physics/scripting/threading disabled. The default Samples executable initialized graphics/ImGui but could not complete startup because XAudio failed to create a master voice. See [Platform.md](Platform.md#validation) for the focused test projects and their limits. This does not establish a full Release engine build or physical input/audio playback validation.

There is a focused Windows platform smoke project, but no general engine test suite or tracked CI workflow. Build the affected projects and exercise relevant behavior in a sample. Do not use third-party test counts as evidence that engine behavior was tested.

| Change | Useful check after the relevant build |
| --- | --- |
| Native platform services | Build and run `Nuclear.Platform/Nuclear.Platform.Tests.vcxproj` as described in [Platform.md](Platform.md#validation), then exercise window resize/close, keyboard/mouse modes, and ImGui in Samples |
| Networking transport | Build and run `Nuclear.Platform/Nuclear.Platform.NetworkTests.vcxproj` in Debug and Release; exercise IPv4/IPv6 UDP/TCP loopback and runtime lifetimes without window startup |
| CPU/GPU/memory profiling | Run the three projects listed in [Profiling.md](Profiling.md#validation); verify query support/validity, bounded history, process counters, explicit accounting, asynchronous GPU results, and shutdown on the backend being changed |
| Core/client lifecycle | Open selector, enter/exit a sample, close application, inspect shutdown log |
| Module manager or audio loading | Build the engine and relevant DLL, then launch a sample with the selected module or audio backend; exercise playback for audio changes |
| Basic mesh/animation | Sample1, subject to local model availability |
| Shaders, PBR, render paths | Sample2; exercise affected rendering toggles and inspect shader diagnostics |
| Physics or scripts | Build the engine, ScriptCore, and SamplesScripts, then exercise Sample3 after verifying content and managed assemblies |
| Camera/render-target sizing | Sample4 and window resizing; inspect both camera outputs |
| Async assets | Sample5 with prepared imported textures; verify completion and resource readiness |
| Editor panels | Build editor, then exercise the specific panel if its runtime prerequisites exist |
| Docs only | Verify relative links/source paths and `git diff --check`; no engine build required |

The selector labels identify sample intent; they do not establish that each sample is runnable from a clean clone. Report the exact configuration, command, sample, and any missing prerequisite. Separate compiler failures, linker/dependency failures, missing runtime content, and behavior regressions.
