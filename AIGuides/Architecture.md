# Architecture and runtime flow

## Startup and client lifecycle

[Samples/Main.cpp](../Samples/Main.cpp) configures `Core::EngineStartupDesc`, starts the engine, registers `@CommonAssets@`, and loads a `SampleSelector` client. [The editor entry point](../Nuclear.Editor/source/Nuclear.Editor.cpp) uses the same engine lifecycle with its own client and asset alias.

[Engine.cpp](../Nuclear.Engine/Source/Core/Engine.cpp) is the central lifecycle implementation:

1. Initialize the asset library and reserved paths.
2. Initialize SDL video and create the window.
3. Conditionally initialize graphics, audio, scripting, PhysX, rendering, threading, and fallback modules, in that order.
4. Initialize the asset manager. The preceding fallback module creates solid black, grey, white, and flat-normal textures synchronously in memory, without external image files.
5. `LoadClient` calls client `Initialize`, then `Load`, then enters the blocking main loop.
6. Each frame handles SDL events, executes up to one queued main-thread task, updates input/timing, starts ImGui, calls client `Update` and `Render`, then renders ImGui and presents.
7. `EndClient` invokes `ExitRendering` and `Shutdown`. Engine shutdown is separate; inspect its actual order when changing resource lifetimes.

`Core::Client` is the application extension point. Its default callbacks are empty: the engine does not automatically update the scene's systems on behalf of every client. Inspect a sample's `Render`/`Update` methods to see what it drives.

Modules commonly expose a singleton `Get()`, either directly or through [EngineModule<T>](../Nuclear.Engine/include/Core/EngineModule.h). This is shared process state; repeated client loading does not create an isolated engine instance.

## Scene, entities, and systems

[Core::Scene](../Nuclear.Engine/Source/Core/Scene.cpp) owns the EnTT registry, system manager, main-camera pointer, and current scene-asset pointer. `Scene::CreateEntity` adds an `EntityInfoComponent`, which carries the transform and name. Prefer that path when dependent components expect entity information to exist.

[ECS::Entity](../Nuclear.Engine/include/ECS/Entity.h) wraps a registry pointer and entity identifier and provides component templates. It does not replace EnTT's validity and lifetime rules. Avoid retaining component pointers across registry clearing or component removal without checking the relevant lifetime.

[SystemManager](../Nuclear.Engine/include/ECS/System.h) holds shared system instances in an `unordered_map`. `Update_All` iterates that map, so insertion order is not a guaranteed simulation order. Systems bridge components to rendering, physics, audio, scripting, and debug facilities.

Scene snapshots use EnTT archives with zpp::bits. `SaveScene` and `LoadScene` currently enumerate only `EntityInfoComponent`, `LightComponent`, and `MeshComponent`. Persistence of a newly added component is not automatic.

## Assets and threading

[AssetManager](../Nuclear.Engine/include/Assets/AssetManager.h) is the public import/load/export facade:

- **Import** handles source content for the first time through `Importer` and format-specific importers.
- **Load** handles previously exported content and its metadata through `Loader`.
- **AssetLibrary** stores typed asset objects in UUID-keyed maps and provides references/pointers into those maps.
- **IAsset** exposes identity, type, and states: `Unknown`, `Deserialized`, `Queued`, `Loaded`, and `Created`.

The texture path is a useful concrete example: [TextureImportTask](../Nuclear.Engine/include/Assets/Tasks/TextureImportTask.h) decodes data and can export `.NEMeta`; it then queues [TextureCreateTask](../Nuclear.Engine/include/Assets/Tasks/TextureCreateTask.h) for graphics resource creation on the main thread. The creation task removes the asset from the importer/loader queue and cleans up temporary image data. Task implementations can self-delete in `OnEnd`.

[ThreadingModule](../Nuclear.Engine/Source/Threading/ThreadingModule.cpp) manages a worker pool and a separate main-thread task list. Read each asset type's implementation before generalizing texture behavior. A pointer returned by import can refer to an object whose asynchronous work is still pending.

Reserved path aliases are expanded by [Core::Path](../Nuclear.Engine/Source/Core/Path.cpp). Engine startup sets `@Assets@`, `@NuclearAssets@`, and `@CurrentPath@`; clients add aliases such as `@CommonAssets@` and `@EditorAssets@`. Relative paths are sensitive to the process working directory.

## Graphics and rendering

- [GraphicsModule](../Nuclear.Engine/Source/Graphics/GraphicsModule.cpp) owns the Diligent device/context/swap-chain integration and resource creation.
- [RenderingModule](../Nuclear.Engine/Source/Rendering/RenderingModule.cpp) provides shared rendering resources, including camera/animation constant buffers and final render targets.
- [RenderSystem](../Nuclear.Engine/Source/Systems/RenderSystem.cpp) connects ECS cameras, meshes, lights, shader registration/baking, and render passes. It iterates active cameras and constructs `FrameRenderData`.
- [ShaderPipeline](../Nuclear.Engine/Source/Graphics/ShaderPipeline.cpp) and the variant factory build pipeline variants and reflection/bindings. Material and mesh state determine which variants and resources are used.
- Render passes and rendering paths live under `Rendering/`. Existing deferred filenames and symbols use the spelling **`Deffered`**.
- [ShaderParser](../Nuclear.Engine/Source/Parsers/ShaderParser.cpp) parses TOML-style `.NuclearShader` descriptions. They can refer to HLSL paths or embed source. Resource names such as `NEMat_Diffuse1` and `NEStatic_Camera` connect shaders to C++ binding logic.

`NE_Common.h` selects a left-handed coordinate convention and zero-to-one clip depth. Check [Math.h](../Nuclear.Engine/include/Math/Math.h), buffer definitions, and shader code together before changing matrix or coordinate handling.

## Scripting, physics, audio, and editor

.NET hosting and assembly loading live in `ScriptingModule`, using the included `Nuclear.Managed` / `Nuclear.Managed.Native` source fork (derived from Coral; see its [provenance](../Nuclear.Managed/README.md)). Both ScriptCore and client assemblies load into one collectible assembly context. The engine uses Nuclear.Managed's type/object API to construct scripts and invoke methods; it does not resolve native exports from client assemblies. All managed projects target .NET 10; native hosting selects the latest stable .NET 10 hostfxr in an installation directory. The bridge uses `Nuclear.Managed` in C# and `Nuclear::Managed` in C++.

`InitBindings` uploads native callbacks to the unmanaged Cdecl function-pointer fields in `Nuclear.NativeCalls`. Strings use Nuclear.Managed's disposable `NativeString`; booleans cross the boundary as 32-bit integers. `ScriptingRegistry` keys component operations by Nuclear.Managed type IDs and registers the implemented EntityInfo and Light wrappers. Unknown types and invalid entity IDs fail without inserting empty callbacks.

`ScriptingSystem` constructs a derived `Entity`, calls `BindEntity` before `OnStart`, and invokes `OnUpdate(float)`. Managed scripts override those virtual lifecycle methods. `ScriptingObject` shares ownership of its managed handle; the module tracks live handles and destroys them before unloading the context at shutdown. Class lifetime tokens reject construction from an unloaded context. CoreCLR itself remains loaded for the process lifetime. Managed wrappers are in `Nuclear.ScriptCore`, and examples are in `SamplesScripts`.

`PhysXModule` owns SDK-level physics objects; `PhysXSystem` owns a simulation scene and connects collider/rigid-body components to actors. `AudioModule` selects a backend; `AudioSystem` connects source/listener components to it. Current engine startup requests XAudio2 even though an OpenAL backend also exists.

The editor uses the same engine and ECS, with inspectors and panels under `Nuclear.Editor/include` and `source`. It is not a separate authoritative scene model. Consult [KnownLimitations.md](KnownLimitations.md) before assuming a complete editor or scripting workflow.
