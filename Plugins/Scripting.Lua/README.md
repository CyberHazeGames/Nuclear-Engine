# Scripting.Lua

This is the package scaffold for a future Lua scripting backend. It has no
Lua dependency, runtime, bindings, asset loader, or registration hook yet.
The placeholder implements the shared backend interface but always declines
initialization, so `ScriptingModule::GetBackendNames()` lists only registered
backends. C# remains the default backend.

When implemented, keep Lua state and Lua-specific handles here and register
the backend with `ScriptingModule::RegisterBackend` before initialization.
