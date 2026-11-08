# Modular Game Engine Architecture

The clean way to evolve a monolithic engine is to make **modules the
primary architectural boundary**, then put **subsystems/services inside
modules**, and make **plugins packages that contribute one or more
modules**.

For NuclearEngine, aim for something conceptually like:

``` text
NuclearEngine
│
├── Engine/
│   ├── Core/
│   ├── Platform/
│   ├── Application/
│   ├── Assets/
│   ├── ECS/
│   ├── Scene/
│   ├── Rendering/
│   ├── Physics/
│   ├── Audio/
│   ├── Input/
│   ├── UI/
│   ├── Scripting/
│   └── Editor/
│
├── Plugins/
│   ├── Graphics.Diligent/
│   ├── Physics.Jolt/
│   ├── Scripting.CSharp/
│   ├── Scripting.Lua/
│   ├── Scripting.JavaScript/
│   └── Importers.FBX/
│
└── Game/
```

The important part is that `Scripting` **does not implement C#, Lua, or
JavaScript**. It defines what a scripting backend *is*. Then
`Scripting.CSharp`, `Scripting.Lua`, etc. implement that interface.

## 1. Think in Four Layers

``` text
Module
   └── owns engine functionality
        └── contains services/subsystems

Plugin
   └── distributable package
        └── contains one or more modules

Extension Point
   └── interface exposed by a module
        └── other modules/plugins implement it

Subsystem
   └── runtime service owned by a module
```

For example:

``` text
Scripting                  <- Engine module
│
├── ScriptEngine
├── ScriptContext
├── ScriptObject
├── IScriptingBackend      <- extension point
└── ScriptBackendRegistry
       ▲
       │
       ├── Scripting.CSharp
       ├── Scripting.Lua
       └── Scripting.JavaScript
```

## 2. Give Every Module a Lifecycle

``` cpp
class IModule
{
public:
    virtual ~IModule() = default;

    virtual void OnLoad() {}
    virtual void OnInitialize() {}
    virtual void OnStart() {}

    virtual void OnUpdate(float deltaTime) {}

    virtual void OnStop() {}
    virtual void OnShutdown() {}
    virtual void OnUnload() {}
};
```

Then:

``` cpp
class ScriptingModule : public IModule
{
public:
    void OnInitialize() override;
    void OnShutdown() override;
};
```

Your engine owns a `ModuleManager`:

``` cpp
class ModuleManager
{
public:
    void RegisterModule(ModuleDescriptor descriptor);

    void LoadModules();
    void InitializeModules();
    void StartModules();

    void UpdateModules(float dt);

    void ShutdownModules();
    void UnloadModules();
};
```

Modules shouldn't manually initialize each other.

Avoid:

``` cpp
Rendering.Initialize();
Physics.Initialize();
Scripting.Initialize();
```

Instead, modules declare dependencies:

``` text
Scripting
    depends on:
        Core
        ECS
        Assets

Scripting.CSharp
    depends on:
        Core
        Scripting
```

The module manager creates a dependency graph and topologically sorts
it:

``` text
Core
 ↓
Assets
 ↓
ECS
 ↓
Scripting
 ↓
Scripting.CSharp
```

## 3. Scripting as an Extension Interface

The base scripting module might expose:

``` cpp
class IScriptingBackend
{
public:
    virtual ~IScriptingBackend() = default;

    virtual std::string_view GetName() const = 0;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual ScriptAssemblyHandle LoadAssembly(
        const std::filesystem::path& path) = 0;

    virtual ScriptClassHandle FindClass(
        ScriptAssemblyHandle assembly,
        std::string_view namespaceName,
        std::string_view className) = 0;

    virtual ScriptObjectHandle CreateInstance(
        ScriptClassHandle type) = 0;

    virtual void Invoke(
        ScriptObjectHandle object,
        std::string_view method) = 0;
};
```

C# implements it:

``` cpp
class CSharpScriptingBackend final : public IScriptingBackend
{
public:
    bool Initialize() override
    {
        // Initialize Coral/.NET runtime
        return true;
    }

    void Shutdown() override
    {
        // Shutdown runtime
    }

    std::string_view GetName() const override
    {
        return "CSharp";
    }
};
```

Lua can provide another implementation:

``` cpp
class LuaScriptingBackend final : public IScriptingBackend
{
public:
    std::string_view GetName() const override
    {
        return "Lua";
    }
};
```

The engine doesn't need to know either exists.

## 4. Use Registries for Extension Points

``` cpp
class ScriptBackendRegistry
{
public:
    void Register(
        std::string name,
        std::unique_ptr<IScriptingBackend> backend);

    void Unregister(std::string_view name);

    IScriptingBackend* Find(std::string_view name);

private:
    std::unordered_map<
        std::string,
        std::unique_ptr<IScriptingBackend>
    > m_Backends;
};
```

Loading plugins produces:

``` text
ScriptBackendRegistry

"CSharp"     → CSharpBackend
"Lua"        → LuaBackend
"JavaScript" → JavaScriptBackend
```

The editor can query the registry instead of hardcoding supported
languages.

## 5. Generalize Extension Points Across the Engine

``` text
Rendering
    IGraphicsBackend
        Graphics.Diligent
        Graphics.Vulkan
        Graphics.DX12

Physics
    IPhysicsBackend
        Physics.Jolt
        Physics.PhysX

Audio
    IAudioBackend
        Audio.FMOD
        Audio.Wwise
        Audio.MiniAudio

Scripting
    IScriptingBackend
        Scripting.CSharp
        Scripting.Lua
        Scripting.JavaScript

Assets
    IAssetImporter
        Importer.FBX
        Importer.GLTF
        Importer.PNG

Editor
    IEditorExtension
        TerrainEditor
        MaterialEditor
        DialogueEditor
```

## 6. Don't Make Everything an Interface

Interfaces are useful where something is genuinely replaceable or
extensible.

Good candidates:

``` text
IScriptingBackend
IGraphicsBackend
IPhysicsBackend
IAudioBackend
IAssetImporter
IAssetCompiler
IEditorPanel
```

Usually unnecessary:

``` text
ITransformComponent
IEntity
IVector3
IWorld
```

These can remain normal engine types.

## 7. Modules Communicate Through Services

Introduce a `ServiceRegistry` or `EngineContext`:

``` cpp
class EngineContext
{
public:
    template<typename T>
    T& Get();

    template<typename T>
    void Register(std::unique_ptr<T> service);

    template<typename T>
    void Unregister();
};
```

A module receives it:

``` cpp
class IModule
{
public:
    virtual void OnInitialize(EngineContext& context) = 0;
};
```

Then:

``` cpp
void CSharpModule::OnInitialize(EngineContext& context)
{
    auto& scripting =
        context.Get<ScriptingService>();

    scripting.RegisterBackend(
        "CSharp",
        std::make_unique<CSharpBackend>()
    );
}
```

This is preferable to recreating the monolith through a giant global
`Engine` singleton.

## 8. Modules vs. Subsystems

**Module = code/package boundary**

**Subsystem = instantiated runtime service**

For example:

``` text
Rendering Module
│
├── RendererSubsystem
├── ShaderSubsystem
├── MaterialSubsystem
└── RenderResourceSubsystem
```

``` text
Assets Module
│
├── AssetManager
├── AssetRegistry
├── AssetCompiler
└── AssetDatabase
```

``` text
Scripting Module
│
├── ScriptSubsystem
├── ScriptRegistry
├── ScriptComponentSystem
└── ScriptBackendRegistry
```

Avoid turning every small system into a separate module. A module should
represent a meaningful architectural boundary.

## 9. Plugins Should Be Packages

Avoid separate module and plugin architectures.

A plugin should simply be a package containing modules, assets,
binaries, resources, and metadata.

``` text
Plugins/
└── Scripting.CSharp/
    │
    ├── Plugin.json
    │
    ├── Source/
    │   ├── Public/
    │   └── Private/
    │
    ├── Binaries/
    ├── Resources/
    └── Managed/
```

Example `Plugin.json`:

``` json
{
    "Name": "Scripting.CSharp",
    "Version": "1.0.0",

    "Modules": [
        {
            "Name": "Scripting.CSharp",
            "Type": "Runtime"
        }
    ],

    "Dependencies": [
        "Core",
        "Scripting"
    ]
}
```

A plugin can contain multiple modules:

``` json
{
    "Name": "TerrainTools",

    "Modules": [
        {
            "Name": "Terrain",
            "Type": "Runtime"
        },
        {
            "Name": "Terrain.Editor",
            "Type": "Editor"
        }
    ]
}
```

## 10. Runtime, Editor, Developer, and Program Modules

Add module types early:

``` cpp
enum class ModuleType
{
    Runtime,
    Editor,
    Developer,
    Program
};
```

For example:

``` text
Scripting                 Runtime
Scripting.CSharp          Runtime
Scripting.CSharp.Editor   Editor

Renderer                  Runtime
Renderer.Editor           Editor

AssetCompiler             Developer
```

Shipping games therefore don't need to include editor-only systems.

## 11. Separate API from Implementation

``` text
Engine/
└── Scripting/
    ├── Public/
    │   ├── IScriptingBackend.h
    │   ├── ScriptEngine.h
    │   ├── ScriptTypes.h
    │   └── ScriptingModule.h
    │
    └── Private/
        ├── ScriptEngine.cpp
        ├── ScriptRegistry.cpp
        └── ScriptingModule.cpp
```

Other modules may include `Scripting/Public/`, but should never include
`Scripting/Private/`.

## 12. Make Dependencies Explicit in the Build System

With Visual Studio projects, modules can map closely to
projects/libraries:

``` text
Nuclear.Core
Nuclear.Platform
Nuclear.ECS
Nuclear.Assets
Nuclear.Rendering
Nuclear.Physics
Nuclear.Audio
Nuclear.Scripting
Nuclear.Editor

Plugins:
Nuclear.Scripting.CSharp
Nuclear.Scripting.Lua
Nuclear.Graphics.Diligent
```

For example:

``` text
Nuclear.Scripting.CSharp
    references:
        Nuclear.Core
        Nuclear.Scripting
```

Only reference additional modules when genuinely required. The compiler
and linker then help enforce architecture boundaries.

## 13. Avoid Circular Module Dependencies

Bad:

``` text
Scene
 ↓
Physics
 ↓
Scene
```

or:

``` text
Rendering
 ↓
Assets
 ↓
Rendering
```

When this occurs, extract a smaller shared abstraction.

Instead of:

``` text
Assets ↔ Rendering
```

use:

``` text
        Core
         │
    RenderTypes
      /      \
Assets      Rendering
```

Likewise, prefer:

``` text
             ECS
            /   \
       Physics   Scene
```

over:

``` text
Physics ↔ Scene
```

## 14. Use Events for Notifications

Not every interaction requires direct module knowledge.

``` cpp
struct AssetReloadedEvent
{
    AssetHandle Asset;
};
```

Then:

``` text
Assets
   │
   │ AssetReloadedEvent
   ▼
EventBus
   │
   ├── Rendering
   ├── Scripting
   └── Editor
```

A useful rule:

``` text
"Do this for me"
        ↓
Service/interface

"This happened"
        ↓
Event
```

## 15. Overall NuclearEngine Architecture

``` text
                         Game
                          │
                 ┌────────┴────────┐
                 │                 │
             Gameplay          Plugins
                                   │
              ┌────────────────────┼──────────────────┐
              │                    │                  │
       Scripting.CSharp      Physics.Jolt      Graphics.Diligent
              │                    │                  │
              ▼                    ▼                  ▼
          Scripting             Physics            Rendering
              │                    │                  │
              └──────────┬─────────┴─────────┬────────┘
                         │                   │
                        ECS                Assets
                         │                   │
                         └─────────┬─────────┘
                                   │
                                  Core
                                   │
                               Platform
```

Editor modules can sit above runtime modules:

``` text
Editor
│
├── Editor.Scene
├── Editor.Assets
├── Editor.Scripting
├── Editor.Materials
└── Editor.Profiler
```

The editor consumes runtime modules. Runtime modules should not depend
on the editor.

## 16. C# as a Complete Example

With Coral:

``` text
Nuclear.Scripting
│
├── IScriptingBackend
├── ScriptType
├── ScriptObject
├── ScriptField
├── ScriptMethod
├── ScriptValue
├── ScriptComponent
└── ScriptBackendRegistry

              ▲
              │ implements
              │
Nuclear.Scripting.CSharp
│
├── CoralHost
├── CSharpBackend
├── CSharpAssembly
├── CSharpType
├── CSharpObject
└── CSharpInterop

              ▲
              │ optional editor extension
              │
Nuclear.Scripting.CSharp.Editor
│
├── CSharpProjectGenerator
├── CSharpCompiler
├── CSharpInspector
└── CSharpDebuggerIntegration
```

Later:

``` text
Nuclear.Scripting.Lua
```

should not require changing `Nuclear.Scripting.CSharp`, and ideally
should not require changing `Nuclear.Scripting` either.

Installing the plugin should simply make the registry discover another
backend:

``` text
Available scripting engines:
    CSharp
    Lua
```

## Migration Strategy

Before turning every current folder into a DLL, establish:

``` text
                    ModuleManager
                         │
            ┌────────────┴────────────┐
            │                         │
      ServiceRegistry            EventBus
            │
            │
     Extension Registries
            │
    ┌───────┼─────────┐
    │       │         │
Scripting Physics  Rendering
Registry  Registry  Registry
```

Then migrate gradually:

``` text
Stage 1
Monolithic NuclearEngine
        +
ModuleManager

Stage 2
Core
Assets
ECS
Rendering
Scripting

Stage 3
Move Diligent out
        ↓
Graphics.Diligent plugin

Stage 4
Move Coral out
        ↓
Scripting.CSharp plugin

Stage 5
Editor modules

Stage 6
External/dynamic plugins
```

You do **not** need DLL hot-loading immediately. Modules can initially
be statically linked libraries while following the same module API.
Dynamic DLL/shared-library loading can be added later without
redesigning the architecture.

## Core Design Principle

> **Engine modules define capabilities and extension points. Plugins
> provide implementations. Subsystems provide runtime services.
> Registries allow implementations to appear without the engine knowing
> about them beforehand.**

This provides Unreal-like modularity without making NuclearEngine's core
dependent on every optional technology.
