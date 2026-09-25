# Change workflows

Use these as change-impact maps, then read the current implementation. They do not require unrelated cleanup or automatically expand the user's task.

## Add or modify native source

1. Find the owning subsystem in [SourceReference.md](SourceReference.md). Inspect its public header, implementation, and an actual caller.
2. Follow neighboring namespace, export, naming, and ownership conventions. `NEAPI` is controlled by `NUCLEARENGINE_EXPORTS` in the engine project.
3. Add new headers/source files to the relevant `.vcxproj` item lists. C# projects likewise use explicit `Compile` items. `.filters` files are ignored here.
4. Check umbrella headers such as `NuclearEngine.h`, `Components/Components.h`, or `Systems/Systems.h` when the new type belongs in the public aggregate API.
5. Build the affected project and consumers when public declarations or binary interfaces changed.

## Add or change an engine module or audio backend

- Derive from `EngineModule` and register through `Engine::GetModuleManager()` before `Start`; declare dependencies by registered name. The manager owns `unique_ptr` registrations and borrows references to longer-lived singleton modules.
- For an external DLL, export `GetNuclearEngineModulePlugin` with the factory table in [EngineModulePlugin.h](../Nuclear.Engine/include/Core/EngineModulePlugin.h). Match the engine architecture, runtime, and C++ ABI; the DLL's destroy callback must destroy each instance it created.
- Use `ModuleManager::LoadPlugin(path)` for an explicit DLL or `EngineStartupDesc.mModulePluginDirectory` for a startup scan of `Nuclear.Module.*.dll` files. Load before `StartModules`, and keep module dependencies valid across DLLs.
- Keep load, initialize, and start phases paired with stop, shutdown, and unload. Initialization failure should leave the module safe for unload; the manager rolls back previously completed phases in reverse dependency order.
- For audio, implement `AudioBackend` in a separate DLL project and export `GetNuclearAudioBackendPlugin` with the structure in `AudioBackendPlugin.h`. Keep its name equal to the suffix in `Nuclear.Audio.<name>.dll`, and build against the matching engine configuration.
- Add the project to the solution, build it with the engine, and exercise module loading or audio playback in a sample with the selected backend.

## Add or change an ECS component/system

- Use `include/Components` and `Source/Components` for component declarations/implementation; system counterparts live in `include/Systems` and `Source/Systems`.
- Trace entity creation and `EntityInfoComponent`/transform assumptions. Check EnTT `on_construct` hooks for required initialization.
- Add systems through `Scene::GetSystemManager()` where the client constructs its scene. `SystemManager::Add` only stores the instance; it does not call a universal initialization routine.
- Decide where initialization, updates, resizing, and teardown occur. Do not rely on deterministic `Update_All` ordering: its storage is an unordered map.
- If persistent, update both scene snapshot component lists plus the serialization adapters/fields. Verify an actual round trip with the relevant components.
- If script-visible or editable, follow the scripting workflow and inspect `EntityEditor.cpp` respectively.

## Add or repair a managed API

Keep these pieces consistent:

1. Public C# wrapper and `delegate* unmanaged[Cdecl]` fields in `Nuclear.ScriptCore/NativeCalls.cs`.
2. C++ declarations in `Plugins/Scripting.CSharp/Public/ScriptingBindings.h` and implementation in `Plugins/Scripting.CSharp/Source/ScriptingBindings.cpp`.
3. Exact `Nuclear.NativeCalls` field names registered through Nuclear.Managed in `CSharpScriptingBackend::InitBindings`.
4. For component add/has support, matching managed type names and `ScriptingRegistry::RegisterEngineComponents` entries.
5. Native/managed value widths, struct layout, argument passing (`ref`/`out` versus pointers/value parameters), object lifetime, and entity validity.
6. Explicit managed project compile entries, then rebuild ScriptCore and SamplesScripts and verify the DLLs in the runtime working directory are current.

Use 32-bit integers for boolean callback results, dispose temporary Nuclear.Managed strings on the allocating side, and keep scalar/pointer parameters consistent. Light intensity is passed by value; colors use pointers to four sequential floats. Build ScriptCore and the C# backend after interop changes, then exercise the affected calls in a sample. Scripts override `Entity.OnStart()` and `Entity.OnUpdate(float)`.

## Change asset import/load behavior

- Follow `AssetManager` -> `Importer` or `Loader` -> the asset-specific task -> main-thread resource creation where applicable.
- Check `ImportingDescs.h`, `LoadingDescs.h`, `AssetMetadata.h`, and the asset's UUID/library mapping.
- Preserve the distinction between disk data being `Loaded` and the resource being `Created`. Test both successful completion and a missing/invalid input.
- Inspect task failure cleanup and queue removal. Raw pointers may refer to library-owned assets or task-owned temporary data; some tasks self-delete.
- If format or metadata changes, update serialization and deserialization together and determine how existing exported content is handled.
- When adding fixture content, inspect `.gitignore`: broad image and generated-content rules can hide files required by the change. Avoid force-adding large imported libraries as incidental validation output.

## Change rendering or shaders

- Trace the affected camera/mesh/material through `RenderSystem`, pipeline baking/reflection, the chosen rendering path, and the relevant pass.
- For `.NuclearShader` changes, inspect `ShaderParser.cpp` and a neighboring description. These are TOML-style files, not raw HLSL alone.
- For shader variable changes, search the exact resource name in both C++ and shaders. Keep static bindings, material naming, reflection, C++ structures, and HLSL constant buffers aligned.
- Check relevant resize handling in camera components, render passes, GBuffer, and `RenderingModule` when changing targets.
- Preserve asset readiness requirements during pipeline baking. Verify the backend actually used; do not claim D3D12/OpenGL/Vulkan coverage from a D3D11 run.
- Use Sample2 for rendering options and Sample4 for multiple-camera behavior when their local assets are available.

## Change dependencies or build settings

- Read `.gitmodules`, the dependency scripts, project settings, and source-level `#pragma comment(lib, ...)` directives together.
- Preserve user edits; record the intended toolset and configuration explicitly. Check the resulting library filenames instead of assuming they match old copy rules.
- Update only the relevant submodule/build integration and document its tested outcome. A dependency's own `AGENTS.md` applies when editing that subtree.
- Keep setup failures separate from engine regressions. Update [BuildAndValidation.md](BuildAndValidation.md) when a build assumption becomes verified or changes.
