# Source-observed limitations

Inspected on 2026-09-21. These are bounded observations for navigation and diagnosis, not an exhaustive bug audit or runtime test report. Recheck the source before relying on an entry, and remove/update entries when fixed.

| Area | Observation and consequence | Evidence |
| --- | --- | --- |
| Build toolsets | First-party native solution projects target Visual Studio 2026 / MSVC v145; third-party library naming and prebuilt outputs may still reference older toolsets and need checking when building dependencies. | [Build guide](BuildAndValidation.md), [dependency script](../RunCmakeForDependencies.bat) |
| CMake | Root module-path syntax is malformed, wiring is incomplete, and Samples names `main.cpp` instead of `Main.cpp`. A portable CMake build is not established. | [Root CMake](../CMakeLists.txt), [engine CMake](../Nuclear.Engine/CMakeLists.txt), [Samples CMake](../Samples/CMakeLists.txt) |
| Copy script | Assimp/PhysX/Diligent filenames and paths are hard-coded; final `exit 0` can conceal copy failures. It does not supply every external dependency. | [Copy script](../CopyDependenciesBinaries.bat) |
| Platform support | Windows export declarations, library pragmas, Win32 integrations, and calls such as `Sleep` remain in engine code. README portability statements are goals, not test evidence. | [NE_Compiler.h](../Nuclear.Engine/include/NE_Compiler.h), [Engine.cpp](../Nuclear.Engine/Source/Core/Engine.cpp), [RenderSystem.cpp](../Nuclear.Engine/Source/Systems/RenderSystem.cpp) |
| Tests | A focused scripting interop smoke check exists outside the main solution; there is no general engine test suite or tracked CI workflow. | [Scripting smoke runner](../BuildSupport/TestScripting.ps1) |
| Scene persistence | Save/load snapshots list only entity info, light, and mesh components. Other component state is not automatically persisted. | [Scene.cpp](../Nuclear.Engine/Source/Core/Scene.cpp), `SaveScene` / `LoadScene` |
| System order | `Update_All` iterates an unordered map; adding systems in a certain order does not establish update order. | [System.h](../Nuclear.Engine/include/ECS/System.h), [System.cpp](../Nuclear.Engine/Source/ECS/System.cpp) |
| Script component coverage | Only EntityInfo and Light have managed component wrappers and registered add/has operations. Other native component types still need managed APIs. | [ScriptingRegistry.cpp](../Nuclear.Engine/Source/Scripting/ScriptingRegistry.cpp), [managed components](../Nuclear.ScriptCore/Components) |
| .NET runtime | Nuclear.Managed selects stable .NET 10 hostfxr from the system installation; the x64 .NET 10 runtime is required. Custom DOTNET_ROOT installations are not discovered. CoreCLR stays loaded until process exit. | [Build guide](BuildAndValidation.md), [ScriptingModule.cpp](../Nuclear.Engine/Source/Scripting/ScriptingModule.cpp) |
| Sample prerequisites | Models, textures, imported assets, and runtime outputs are substantially ignored/local. The LearnOpenGL content subset does not supply all models used by Sample1 and the selector. Check exact paths before diagnosing runtime behavior. | [.gitignore](../.gitignore), [content setup](BuildAndValidation.md#learnopengl-sample-content) |
| Editor setup | Startup requests `EditorClient.dll`; the solution has no project producing that assembly. Editor completeness cannot be inferred from the presence of its native project. | [Editor startup](../Nuclear.Editor/source/Nuclear.Editor.cpp), [solution](../Nuclear%20Engine.sln) |

The initial guide-writing pass did not build or launch the engine. Subsequent Debug x64 build and sample startup validation, including remaining sample-content and rendering warnings, is recorded in the [build guide](BuildAndValidation.md#running-samples).
