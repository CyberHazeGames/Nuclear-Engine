# Build and validation

## What the repository currently specifies

Use [Nuclear Engine.sln](../Nuclear%20Engine.sln) as the concrete Windows build entry point. It includes the engine DLL, Samples, editor, ScriptCore, SamplesScripts, and Nuclear.Managed. Engine native projects use C++20 and Debug/Release x64 configurations; Nuclear.Managed.Native uses C++17. Managed projects target .NET 10 and map to Any CPU in the solution; the native host and installed runtime must be x64.

Toolset details are not uniform:

- [RunCmakeForDependencies.bat](../RunCmakeForDependencies.bat) selects `Visual Studio 18 2026` and x64 for Assimp, Diligent, and OpenAL. Diligent and OpenAL also receive `CMAKE_POLICY_VERSION_MINIMUM=3.5`.
- The solution and first-party native projects (engine, editor, samples, Nuclear.Managed.Native, and scripting smoke fixture) target Visual Studio 18 / **v145** for x64 Debug and Release. The installed MSVC toolset inspected on 2026-09-23 was 14.51.36231.
- Dependency copy paths and `#pragma comment(lib, ...)` directives include older toolset-specific names such as `assimp-vc143-mt` and PhysX `vc142` output directories.

Inspect installed toolsets and generated filenames before deciding whether to adjust anything. A generator update alone does not update import-library names, dependency output paths, or all native projects.

## Dependency preparation

The repository does not provide a single complete bootstrap command. For setup work:

1. Inspect `git submodule status` and `.gitmodules`; preserve local submodule changes. If initialization is needed for the task, use `git submodule update --init --recursive` without `--remote` to obtain recorded revisions.
2. Read the dependency batch scripts before running them. They use repository-relative paths and interactive `pause` statements.
3. Configure Assimp, Diligent, and OpenAL using the commands in `RunCmakeForDependencies.bat`, then build the required configurations. Configuration alone does not compile the libraries.
4. Supply/build the remaining dependencies: PhysX, FreeImage, FreeType, msdf-atlas-gen/msdfgen, SDL2, and libsndfile. For scripting, install the x64 .NET 10 runtime and .NET 10 SDK. Nuclear.Managed is included as source; no scripting submodule is required. Inspect project include paths and native library pragmas for the actual expected layout.
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

The solution build includes managed project dependencies and their post-build copying into `Samples/`. Native and managed build outputs go to `Bin/DebugX64/` or `Bin/ReleaseX64/`. Engine native intermediates are under project-local `Build/` folders; Nuclear.Managed.Native and scripting smoke intermediates are under the root `Build/`.

### .NET scripting dependencies and checks

`Nuclear.Managed` is an included source fork of Coral, with upstream revision and MIT attribution recorded in its [README](../Nuclear.Managed/README.md). The [native project](../Nuclear.Managed.Native/Nuclear.Managed.Native.vcxproj) builds `Nuclear.Managed.Native.lib`; the [managed project](../Nuclear.Managed/Nuclear.Managed.csproj) builds `Nuclear.Managed.dll` for `net10.0`. No Coral submodule or Premake step is required. The host parses complete numeric versions and selects the highest stable .NET 10 hostfxr in an installation directory. Windows uses the system installation under Program Files; custom `DOTNET_ROOT` discovery is not implemented.

```powershell
dotnet build SamplesScripts/SamplesScripts.csproj -c Debug /m:1 /nr:false
./BuildSupport/TestScripting.ps1 -Configuration Debug
./BuildSupport/TestScripting.ps1 -Configuration Release
```

ScriptCore's build copies `Nuclear.Managed.dll`, its runtime configuration/dependency manifests, and ScriptCore's runtime files into `Samples/`; SamplesScripts copies its own assembly and dependency manifest there. No separate runtime-directory discovery or legacy .NET Framework targeting pack is required.

The smoke check uses real Nuclear.Managed hosting and ScriptCore with a small native callback fixture, without graphics or asset dependencies. It verifies .NET 10 runtime and Nuclear.Managed assembly identity, host version parsing, entity IDs before startup, managed/native strings, component type IDs, colors, intensity, input results, update arguments, garbage collection, exception reporting, and unloading/reloading the assembly context. It does not validate the engine's ECS callbacks or a rendered sample.

Nuclear.Managed/.NET 10 validation on 2026-09-23: `BuildSupport/TestScripting.ps1` passed in Debug and Release, and `BuildSupport/Build.ps1 -Configuration Debug` built the full solution. The smoke runner uses one worker for both managed and native builds, the 64-bit Visual Studio MSBuild, and normalized process Path entries. Existing compiler and native dependency warnings remain; a full Release solution build has not been verified.

Before the Nuclear.Managed/.NET 10 migration, validation on 2026-09-22: managed Debug/Release builds and native Debug/Release smoke checks passed. On 2026-09-23, Assimp Release built with MSVC 14.51. After fixing the engine library search path and Diligent format-conversion dependencies, `BuildSupport/Build.ps1 -Configuration Debug` built the full solution successfully, including the engine DLL, Samples, editor, and managed assemblies. Existing compiler warnings and native dependency debug-symbol/runtime-library warnings remain. A full Release engine build has not been verified.

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

## Choosing validation

The focused first-party scripting smoke check is under `Tests/ScriptingSmoke/` and is run through `BuildSupport/TestScripting.ps1`; it is separate from the main solution. There is no general engine test suite or tracked CI workflow. Do not use third-party test counts as evidence that engine behavior was tested.

| Change | Useful check after the relevant build |
| --- | --- |
| Core/client lifecycle | Open selector, enter/exit a sample, close application, inspect shutdown log |
| Basic mesh/animation | Sample1, subject to local model availability |
| Shaders, PBR, render paths | Sample2; exercise affected rendering toggles and inspect shader diagnostics |
| Physics or scripts | Run the scripting smoke check for interop, then Sample3 after verifying content and managed assemblies |
| Camera/render-target sizing | Sample4 and window resizing; inspect both camera outputs |
| Async assets | Sample5 with prepared imported textures; verify completion and resource readiness |
| Editor panels | Build editor, then exercise the specific panel if its runtime prerequisites exist |
| Docs only | Verify relative links/source paths and `git diff --check`; no engine build required |

The selector labels identify sample intent; they do not establish that each sample is runnable from a clean clone. Report the exact configuration, command, sample, and any missing prerequisite. Separate compiler failures, linker/dependency failures, missing runtime content, and behavior regressions.
