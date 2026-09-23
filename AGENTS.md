# Repository guidance for AI agents

## Start here

This is Nuclear Engine: a work-in-progress C++20 3D engine with an EnTT-based ECS, Diligent rendering, .NET/C# scripting through Nuclear.Managed, PhysX, and an ImGui editor. The concrete build workflow is Windows x64 with Visual Studio/MSBuild. Cross-platform support is an intention, not a verified property of the current source.

1. Read [AIGuides/README.md](AIGuides/README.md) and the guide relevant to the task.
2. Check `git status --short` and inspect existing diffs before editing. Preserve unrelated user changes and dirty submodules.
3. Use [SourceReference.md](AIGuides/SourceReference.md) to find the owning subsystem, then read its current implementation and callers. Source takes precedence over these notes.
4. Read [BuildAndValidation.md](AIGuides/BuildAndValidation.md) before configuring, building, or running anything.

## Working rules

- Tool calls MUST use at least a 10-minute timeout where an execution-timeout parameter is available.
- `wait_agent` calls are non-blocking and are interrupted when a subagent responds or a new user message arrives; they do not count as blocking waits.
- Keep changes focused on the requested behavior. Do not retarget toolsets, update dependencies, rename existing APIs, or reformat unrelated files as incidental cleanup.
- Public engine headers live in `Nuclear.Engine/include/`; implementations live in `Nuclear.Engine/Source/`. Preserve the existing directory and filename casing.
- Match nearby C++ style: `Nuclear` subsystem namespaces, PascalCase types/methods, commonly `m` members and `p` pointer members, `#pragma once`, and `NEAPI` for exported interfaces. Formatting varies; follow the file being edited.
- Native `.vcxproj` and managed `.csproj` files explicitly enumerate source files. Add new compilation units to the appropriate project. CMake is incomplete and is not a replacement for these project files.
- Follow existing ownership and lifecycle boundaries. Assets returned from library maps are not caller-owned. Some task objects delete themselves in `OnEnd`; inspect this before changing allocation or cleanup.
- Keep asset decoding and main-thread resource creation consistent with existing task flows. Do not assume a non-null asset pointer or `Loaded` state means GPU resources are ready.
- For script APIs, check C# declarations, C++ bindings, Nuclear.Managed registration, and component registration together. The managed API is only partially implemented.
- Keep shader resource names, C++ buffer layouts, reflection, and `.NuclearShader` descriptions aligned. Preserve existing `Deffered` spelling in paths and symbols unless a coordinated rename is requested.
- Treat scene/asset serialization changes as format changes; inspect both reader and writer and consider existing assets.
- Limit vendor changes to tasks that require them. `External/` includes submodules and local SDKs; bundled vendor code also exists under engine `ThirdParty`, `Math/glm`, `ECS/entt`, `spdlog`, ImGui, and TOML parser files. Read any nested `AGENTS.md` before editing there.
- Do not commit generated builds, runtime DLLs, logs, imported asset output, or machine-specific IDE state. Check `.gitignore` before assuming a missing file is absent from the local workspace.

## Validation and handoff

- Use the smallest meaningful build and sample smoke check for the change. Scripting has a focused smoke check in `BuildSupport/TestScripting.ps1`; vendor test suites are separate.
- Documentation-only changes need link/path and diff checks, not a dependency rebuild.
- Distinguish verified results, source-derived expectations, and environment blockers. Never report a successful engine build or launch without running it.
- Update the relevant `AIGuides` page when changing architecture, build commands, public source locations, or a documented limitation. Keep this file short and put details in the guides.
