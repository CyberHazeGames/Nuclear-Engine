# Source reference

Start with the owning row, read the declaration and implementation, then search the named symbols/callers. Links use stable paths instead of line numbers. This is a curated first-party source map; it is not a generated index of vendored code.

## Entry points and core

| Topic | Declaration / implementation / example | Useful symbols or responsibilities |
| --- | --- | --- |
| Public engine API | [NuclearEngine.h](../Nuclear.Engine/include/NuclearEngine.h), [NE_Common.h](../Nuclear.Engine/include/NE_Common.h), [NE_Compiler.h](../Nuclear.Engine/include/NE_Compiler.h) | Aggregate includes, integer aliases, coordinate flags, `NEAPI` |
| Engine lifecycle | [Engine.h](../Nuclear.Engine/include/Core/Engine.h), [Engine.cpp](../Nuclear.Engine/Source/Core/Engine.cpp) | `EngineStartupDesc`, `Start`, `LoadClient`, `MainLoop`, `EndClient`, `Shutdown` |
| Client extension point | [Client.h](../Nuclear.Engine/include/Core/Client.h), [Client.cpp](../Nuclear.Engine/Source/Core/Client.cpp), [SampleBase.h](../Samples/SampleBase.h) | Lifecycle, input/resize callbacks, shared scene/asset access |
| Sample executable | [Main.cpp](../Samples/Main.cpp), [SampleSelector.h](../Samples/SampleSelector.h) | Startup paths, managed assembly name, sample selection, asset-library import buttons |
| Scene and persistence | [Scene.h](../Nuclear.Engine/include/Core/Scene.h), [Scene.cpp](../Nuclear.Engine/Source/Core/Scene.cpp) | Entity helpers, registry, main camera, `SaveScene` / `LoadScene` |
| Entity and systems | [Entity.h](../Nuclear.Engine/include/ECS/Entity.h), [System.h](../Nuclear.Engine/include/ECS/System.h), [System.cpp](../Nuclear.Engine/Source/ECS/System.cpp) | Component templates, `SystemManager::Add`, `Update_All` |
| Transforms | [Transform.h](../Nuclear.Engine/include/ECS/Transform.h), [Transform.cpp](../Nuclear.Engine/Source/ECS/Transform.cpp), [EntityInfoComponent.h](../Nuclear.Engine/include/Components/EntityInfoComponent.h) | Spatial state and entity metadata |
| Paths and IDs | [Path.cpp](../Nuclear.Engine/Source/Core/Path.cpp), [UUID.h](../Nuclear.Engine/include/Core/UUID.h), [UUID.cpp](../Nuclear.Engine/Source/Core/UUID.cpp) | Reserved-path expansion and persistent asset identifiers |
| Platform services | [Window.cpp](../Nuclear.Engine/Source/Platform/Window.cpp), [Input.cpp](../Nuclear.Engine/Source/Platform/Input.cpp), [FileSystem.cpp](../Nuclear.Engine/Source/Platform/FileSystem.cpp) | SDL window/input, file operations |
| Diagnostics | [Logger.h](../Nuclear.Engine/include/Utilities/Logger.h), [Logger.cpp](../Nuclear.Engine/Source/Utilities/Logger.cpp), [Profiler.h](../Nuclear.Engine/include/Profiling/Profiler.h) | `NUCLEAR_*`, `CLIENT_*`, log sinks, profiling helpers |

## Assets, jobs, and serialization

| Topic | Primary source | Follow-up |
| --- | --- | --- |
| Asset facade | [AssetManager.h](../Nuclear.Engine/include/Assets/AssetManager.h), [AssetManager.cpp](../Nuclear.Engine/Source/Assets/AssetManager.cpp) | `Import`, `Load`, `Export`, `LoadFolder`, `FlushContainers` |
| Ownership and state | [AssetLibrary.h](../Nuclear.Engine/include/Assets/AssetLibrary.h), [AssetLibrary.cpp](../Nuclear.Engine/Source/Assets/AssetLibrary.cpp), [IAsset.h](../Nuclear.Engine/include/Assets/IAsset.h) | UUID maps, library lifetime, asset readiness |
| Import / load orchestration | [Importer.cpp](../Nuclear.Engine/Source/Assets/Importer.cpp), [Loader.cpp](../Nuclear.Engine/Source/Assets/Loader.cpp) | Dispatch, queued assets, dependent content |
| Import/load descriptions | [ImportingDescs.h](../Nuclear.Engine/include/Assets/ImportingDescs.h), [LoadingDescs.h](../Nuclear.Engine/include/Assets/LoadingDescs.h), [AssetMetadata.h](../Nuclear.Engine/include/Assets/AssetMetadata.h) | Async options, export paths, metadata fields |
| Format adapters | [AssimpImporter.cpp](../Nuclear.Engine/Source/Assets/Importers/AssimpImporter.cpp), [AssimpManager.cpp](../Nuclear.Engine/Source/Assets/Importers/AssimpManager.cpp), [TextureImporter.cpp](../Nuclear.Engine/Source/Assets/Importers/TextureImporter.cpp) | Mesh/material/animation conversion and FreeImage decode |
| Asset tasks | [Tasks directory](../Nuclear.Engine/include/Assets/Tasks), [TextureImportTask.h](../Nuclear.Engine/include/Assets/Tasks/TextureImportTask.h), [TextureCreateTask.h](../Nuclear.Engine/include/Assets/Tasks/TextureCreateTask.h) | Header-defined worker and main-thread tasks; inspect failure cleanup |
| Worker execution | [ThreadingModule.cpp](../Nuclear.Engine/Source/Threading/ThreadingModule.cpp), [ThreadPool.cpp](../Nuclear.Engine/Source/Threading/ThreadPool.cpp), [Task.cpp](../Nuclear.Engine/Source/Threading/Task.cpp) | `AddTask`, `AddMainThreadTask`, `ExecuteMainThreadTasks`, task lifecycle |
| Metadata / binary persistence | [SerializationModule.cpp](../Nuclear.Engine/Source/Serialization/SerializationModule.cpp), [AssetsSerialization.h](../Nuclear.Engine/include/Serialization/AssetsSerialization.h), [SceneArchive.h](../Nuclear.Engine/include/Serialization/SceneArchive.h) | INI metadata, asset adapters, EnTT/zpp::bits archive bridge |
| Fallback resources | [FallbacksModule.cpp](../Nuclear.Engine/Source/Fallbacks/FallbacksModule.cpp), [DefaultMeshes.cpp](../Nuclear.Engine/Source/Assets/DefaultMeshes.cpp) | Default resources and primitive meshes |

## Graphics, materials, and animation

| Topic | Primary source | Follow-up |
| --- | --- | --- |
| Backend/device | [GraphicsModule.h](../Nuclear.Engine/include/Graphics/GraphicsModule.h), [GraphicsModule.cpp](../Nuclear.Engine/Source/Graphics/GraphicsModule.cpp), [GraphicsModuleDesc.h](../Nuclear.Engine/include/Graphics/GraphicsModuleDesc.h) | Diligent creation, default D3D11 selection, device/context/swap chain |
| ECS renderer | [RenderSystem.h](../Nuclear.Engine/include/Systems/RenderSystem.h), [RenderSystem.cpp](../Nuclear.Engine/Source/Systems/RenderSystem.cpp) | `Bake`, `RegisterShader`, `AddRenderPass`, `Update`, lights and camera iteration |
| Shared render state | [RenderingModule.cpp](../Nuclear.Engine/Source/Rendering/RenderingModule.cpp), [FrameRenderData.h](../Nuclear.Engine/include/Rendering/FrameRenderData.h) | Shared constant buffers, final targets, per-camera frame state |
| Camera and GPU structs | [CameraComponent.cpp](../Nuclear.Engine/Source/Components/CameraComponent.cpp), [ShaderStructs.h](../Nuclear.Engine/include/Components/ShaderStructs.h), [LightComponent.cpp](../Nuclear.Engine/Source/Components/LightComponent.cpp) | Projection, render targets, buffer layouts, light state |
| Shader parsing | [ShaderParser.cpp](../Nuclear.Engine/Source/Parsers/ShaderParser.cpp), [ShaderTypes.h](../Nuclear.Engine/include/Graphics/ShaderTypes.h), [DiffuseOnly.NuclearShader](../Assets/NuclearEngine/Shaders/DiffuseOnly.NuclearShader) | TOML descriptions, PSO settings, inline/path-based shader source |
| Pipeline variants | [ShaderPipeline.cpp](../Nuclear.Engine/Source/Graphics/ShaderPipeline.cpp), [ShaderPipelineVariantFactory.cpp](../Nuclear.Engine/Source/Graphics/ShaderPipelineVariantFactory.cpp), [ShaderReflection.h](../Nuclear.Engine/include/Graphics/ShaderReflection.h), [ShaderPipelineSwitch.cpp](../Nuclear.Engine/Source/Graphics/ShaderPipelineSwitch.cpp) | Baking, variant selection, reflection and bindings |
| Mesh/material draw state | [MeshComponent.cpp](../Nuclear.Engine/Source/Components/MeshComponent.cpp), [Material.cpp](../Nuclear.Engine/Source/Assets/Material.cpp), [Mesh.cpp](../Nuclear.Engine/Source/Assets/Mesh.cpp), [RenderingPath.cpp](../Nuclear.Engine/Source/Rendering/RenderingPath.cpp) | Mesh render queue, material textures, draw submission |
| Forward/deferred paths | [ForwardRenderingPath.cpp](../Nuclear.Engine/Source/Rendering/RenderingPaths/ForwardRenderingPath.cpp), [DefferedRenderingPath.cpp](../Nuclear.Engine/Source/Rendering/RenderingPaths/DefferedRenderingPath.cpp), [GBuffer.cpp](../Nuclear.Engine/Source/Rendering/GBuffer.cpp) | Target selection, geometry buffers, path-specific rendering |
| Render passes | [RenderPass.h](../Nuclear.Engine/include/Rendering/RenderPass.h), [passes directory](../Nuclear.Engine/Source/Rendering/RenderPasses), [GeometryPass.cpp](../Nuclear.Engine/Source/Rendering/RenderPasses/GeometryPass.cpp), [ShadowPass.cpp](../Nuclear.Engine/Source/Rendering/RenderPasses/ShadowPass.cpp) | Pass contracts, geometry, deferred lighting, shadows, AO, post-processing |
| IBL/post effects | [ImageBasedLighting.cpp](../Nuclear.Engine/Source/Rendering/ImageBasedLighting.cpp), [Bloom.cpp](../Nuclear.Engine/Source/Rendering/PostProcessingEffects/Bloom.cpp), [PBR shaders](../Assets/NuclearEngine/Shaders/PBR) | Environment capture, BRDF resources, bloom, PBR shader descriptions |
| Animation | [Animator.cpp](../Nuclear.Engine/Source/Animation/Animator.cpp), [AnimationClip.cpp](../Nuclear.Engine/Source/Animation/AnimationClip.cpp), [Bone.cpp](../Nuclear.Engine/Source/Animation/Bone.cpp), [Animations.h](../Nuclear.Engine/include/Assets/Animations.h) | Imported clips, bone transforms, playback |

## Scripting, physics, audio, and editor

| Topic | Primary source | Follow-up |
| --- | --- | --- |
| .NET hosting | [ScriptingModule.cpp](../Nuclear.Engine/Source/Scripting/ScriptingModule.cpp), [ScriptingAssembly.cpp](../Nuclear.Engine/Source/Scripting/ScriptingAssembly.cpp), [Nuclear.Managed.Native build](../Nuclear.Managed.Native/Nuclear.Managed.Native.vcxproj) | Nuclear.Managed host/context, `Initialize`, `CreateScriptingAssembly`, `InitBindings`, managed handle cleanup |
| .NET bridge fork | [HostInstance.hpp](../Nuclear.Managed.Native/include/Nuclear/Managed/HostInstance.hpp), [HostInstance.cpp](../Nuclear.Managed.Native/Source/HostInstance.cpp), [HostFXRDiscovery.hpp](../Nuclear.Managed.Native/Source/HostFXRDiscovery.hpp), [ManagedHost.cs](../Nuclear.Managed/Source/ManagedHost.cs) | .NET 10 host discovery, `Nuclear.Managed` assembly loading, `Nuclear::Managed` native API; [provenance](../Nuclear.Managed/README.md) |
| Native bindings | [ScriptingBindings.h](../Nuclear.Engine/include/Scripting/ScriptingBindings.h), [ScriptingBindings.cpp](../Nuclear.Engine/Source/Scripting/ScriptingBindings.cpp), [ScriptingRegistry.cpp](../Nuclear.Engine/Source/Scripting/ScriptingRegistry.cpp) | Internal-call implementations and component type maps |
| Script lifecycle | [ScriptingSystem.cpp](../Nuclear.Engine/Source/Systems/ScriptingSystem.cpp), [ScriptComponent.cpp](../Nuclear.Engine/Source/Components/ScriptComponent.cpp), [Script.cpp](../Nuclear.Engine/Source/Assets/Script.cpp), [ScriptingObject.cpp](../Nuclear.Engine/Source/Scripting/ScriptingObject.cpp) | Construction/start/update callbacks, object invocation |
| Managed API | [Entity.cs](../Nuclear.ScriptCore/ECS/Entity.cs), [LightComponent.cs](../Nuclear.ScriptCore/Components/LightComponent.cs), [Input.cs](../Nuclear.ScriptCore/Platform/Input.cs), [Sample3.cs](../SamplesScripts/Sample3.cs) | Managed entity/component API, internal calls, example behavior |
| Physics | [PhysXModule.cpp](../Nuclear.Engine/Source/PhysX/PhysXModule.cpp), [PhysXSystem.cpp](../Nuclear.Engine/Source/Systems/PhysXSystem.cpp), [ColliderComponent.cpp](../Nuclear.Engine/Source/Components/ColliderComponent.cpp), [RigidBodyComponent.cpp](../Nuclear.Engine/Source/Components/RigidBodyComponent.cpp) | SDK objects, simulation scene, actor/shape setup and transform sync |
| Audio | [AudioModule.cpp](../Nuclear.Engine/Source/Audio/AudioModule.cpp), [AudioSystem.cpp](../Nuclear.Engine/Source/Systems/AudioSystem.cpp), [AudioBackend.h](../Nuclear.Engine/include/Audio/AudioBackend.h), [XAudio backend](../Nuclear.Engine/Source/Audio/XAudio/XAudioBackend.cpp), [OpenAL backend](../Nuclear.Engine/Source/Audio/OpenAL/OpenALBackend.cpp) | Backend selection, listener/source updates, audio resources |
| Editor entry/client | [Nuclear.Editor.cpp](../Nuclear.Editor/source/Nuclear.Editor.cpp), [Nuclear.Editor.h](../Nuclear.Editor/include/Nuclear.Editor.h), [Project.cpp](../Nuclear.Editor/source/Project.cpp) | Editor startup, engine client, project handling |
| Editor UI | [EditorUI.cpp](../Nuclear.Editor/source/EditorUI.cpp), [EntityEditor.cpp](../Nuclear.Editor/source/EntityEditor.cpp), [AssetLibraryViewer.cpp](../Nuclear.Editor/source/AssetLibraryViewer.cpp), [LoggerView.cpp](../Nuclear.Editor/source/UILayers/LoggerView.cpp) | Panels, component inspection, asset browsing, logs |

## Search recipes

Run from the repository root. Narrow the directory once you know the subsystem; avoid scanning `External/` for ordinary engine work.

```powershell
rg -n 'LoadClient|ExecuteMainThreadTasks|EndClient' Nuclear.Engine/Source/Core/Engine.cpp
rg -n 'RegisterShader|AddRenderPass|Bake\(' Samples Nuclear.Engine/Source/Systems
rg -n 'REGISTER_CALL|RegisterComponent' Nuclear.Engine/Source/Scripting
rg -n 'NativeCalls|delegate\* unmanaged' Nuclear.ScriptCore
rg -n 'NEStatic_Camera|NEMat_Diffuse1' Nuclear.Engine/Source Assets/NuclearEngine/Shaders
rg -n '#pragma comment\(lib' Nuclear.Engine/Source -g '!ThirdParty/**' -g '!Graphics/ImGUI/**'
git ls-files Tests Programs .github
```

For a full first-party file listing, exclude bundled libraries explicitly:

```powershell
rg --files Nuclear.Engine Nuclear.Editor Nuclear.ScriptCore Samples SamplesScripts Assets/NuclearEngine -g '!**/ThirdParty/**' -g '!**/glm/**' -g '!**/entt/**' -g '!**/spdlog/**' -g '!**/ImGUI/**' -g '!**/Parsers/impl/**'
```
