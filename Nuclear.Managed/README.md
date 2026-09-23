# Nuclear.Managed

Nuclear Engine's .NET 10 scripting bridge. The C# assembly and namespace are
`Nuclear.Managed`; the companion [native project](../Nuclear.Managed.Native/Nuclear.Managed.Native.vcxproj)
builds `Nuclear.Managed.Native.lib` with the `Nuclear::Managed` namespace and
public headers under `Nuclear.Managed.Native/include/Nuclear/Managed/`.

This is a source fork of [Studio Cherno's Coral](https://github.com/StudioCherno/Coral),
revision `d53b2685725f7535bc4d1deaa8a22bf16d112fe2`. The original MIT copyright
and license are retained in [LICENSE](LICENSE) and the native directory's LICENSE.
The imported subset contains the managed bridge, native library, and bundled .NET
hosting headers; upstream examples, test projects, and Premake/CMake build files
are not part of the engine build. The engine no longer needs a Coral submodule.

The fork renames namespaces, public host settings/status names, internal symbols,
projects, and runtime files. Native hosting selects the highest stable .NET 10
hostfxr version in an installation directory, parsing the complete version
numerically. It loads `Nuclear.Managed.runtimeconfig.json` and `Nuclear.Managed.dll`.
Windows x64 is the validated build path; other inherited platforms are unverified.
The host uses the system installation under Program Files on Windows; custom
`DOTNET_ROOT` locations and self-contained deployment are not implemented.

Build and validate with `BuildSupport/TestScripting.ps1 -Configuration Debug`
and `-Configuration Release`. See [the engine build guide](../AIGuides/BuildAndValidation.md)
for the full solution workflow. Rebuild client assemblies after migrating from
Coral: their bridge assembly and type references have changed.
