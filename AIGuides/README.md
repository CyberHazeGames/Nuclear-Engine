# AI guides

These notes help future agents navigate and change this repository without repeating the initial investigation. They describe source inspected on **2026-09-21**, based on commit **`5c56558e`** plus the working tree at inspection time. They are navigation aids, not a declaration that every subsystem builds or works.

| Guide | Read it when |
| --- | --- |
| [Architecture.md](Architecture.md) | Understanding startup, ECS, rendering, assets, and runtime boundaries |
| [BuildAndValidation.md](BuildAndValidation.md) | Configuring dependencies, building, launching samples, or diagnosing setup failures |
| [ChangeWorkflows.md](ChangeWorkflows.md) | Adding components, script bindings, assets, shaders, or source files |
| [SourceReference.md](SourceReference.md) | Finding a subsystem's files, entry points, or related implementation |
| [KnownLimitations.md](KnownLimitations.md) | Checking source-observed gaps and avoiding stale assumptions |

Read the root [AGENTS.md](../AGENTS.md) first. For a small task, follow the relevant source-reference row and workflow rather than reading every guide.

## Repository boundaries

| Location | Role |
| --- | --- |
| `Nuclear.Engine/` | Native engine DLL, public headers, implementations, and some bundled libraries |
| `Nuclear.Editor/` | Native ImGui editor application; still work in progress |
| `Nuclear.Managed/`, `Nuclear.Managed.Native/` | Included .NET 10 bridge source fork; C# assembly and native host library |
| `Nuclear.ScriptCore/` | C# engine-facing API, targeting .NET 10 through Nuclear.Managed |
| `Samples/` | Native sample executable and mostly header-defined sample clients |
| `SamplesScripts/` | C# sample assembly, targeting .NET 10 |
| `Assets/NuclearEngine/` | Engine shader descriptions, HLSL, and fonts |
| `Assets/Common/` | Sample content; substantial model/texture content is ignored or supplied locally |
| `External/` | Git submodules, local SDKs, and ignored dependency build trees |
| `Bin/`, `Build/`, `.vs/` | Local output or IDE state; not source of truth |
| `BuildSupport/`, `Tests/ScriptingSmoke/` | Build runners and a focused native/managed scripting smoke check |

## Keeping this reference useful

- Prefer stable file links and symbol names to line numbers that drift.
- Verify a claim against source before relying on it; remove resolved limitations when the corresponding implementation changes.
- Record build/test commands and actual outcomes when validating a future code change. Do not turn an untested suggested command into a claimed known-good command.
- Keep transient machine state and task history out of general instructions. The toolset note in the build guide explicitly distinguishes the inspected working tree from the committed project.
