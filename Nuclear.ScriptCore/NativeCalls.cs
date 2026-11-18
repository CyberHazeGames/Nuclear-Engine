using Nuclear.Managed.Interop;
using Nuclear.Graphics;
using Nuclear.Platform;

namespace Nuclear
{
    // Populated once by Nuclear.Managed before any client script is constructed.
    internal static unsafe class NativeCalls
    {
#pragma warning disable CS0649
        internal static delegate* unmanaged[Cdecl]<NativeString, void> LoggerTrace;
        internal static delegate* unmanaged[Cdecl]<NativeString, void> LoggerInfo;
        internal static delegate* unmanaged[Cdecl]<NativeString, void> LoggerWarn;
        internal static delegate* unmanaged[Cdecl]<NativeString, void> LoggerError;
        internal static delegate* unmanaged[Cdecl]<NativeString, void> LoggerFatal;
        internal static delegate* unmanaged[Cdecl]<uint, ReflectionType, uint> AddComponent;
        internal static delegate* unmanaged[Cdecl]<uint, ReflectionType, uint> HasComponent;
        internal static delegate* unmanaged[Cdecl]<uint, NativeString> EntityGetName;
        internal static delegate* unmanaged[Cdecl]<uint, NativeString, void> EntitySetName;
        internal static delegate* unmanaged[Cdecl]<Input.KeyCode, uint> IsKeyPressed;
        internal static delegate* unmanaged[Cdecl]<uint, Color*, void> LightGetColor;
        internal static delegate* unmanaged[Cdecl]<uint, Color*, void> LightSetColor;
        internal static delegate* unmanaged[Cdecl]<uint, float> LightGetIntensity;
        internal static delegate* unmanaged[Cdecl]<uint, float, void> LightSetIntensity;
        internal static delegate* unmanaged[Cdecl]<NativeString, uint, ulong> OpenScreen;
        internal static delegate* unmanaged[Cdecl]<ulong, NativeString, uint, uint> ScreenButton;
        internal static delegate* unmanaged[Cdecl]<ulong, uint> PollScreen;
        internal static delegate* unmanaged[Cdecl]<uint, NativeString> ScriptFields;
        internal static delegate* unmanaged[Cdecl]<Math.Vector3*, Math.Vector3*, float, float*, uint> Raycast;
        internal static delegate* unmanaged[Cdecl]<uint, int, float, uint> PlayClip;
        internal static delegate* unmanaged[Cdecl]<uint, Math.Vector3*, uint> SetVelocity;
        internal static delegate* unmanaged[Cdecl]<uint, uint, uint> AudioControl;
        internal static delegate* unmanaged[Cdecl]<uint, Gamepad*, uint> ReadGamepad;
        internal static delegate* unmanaged[Cdecl]<uint, NativeString> GameName;
        internal static delegate* unmanaged[Cdecl]<NativeString, uint> LoadLevel;
        internal static delegate* unmanaged[Cdecl]<uint, uint, uint> SetPaused;
        internal static delegate* unmanaged[Cdecl]<NativeString, Math.Vector3*, uint> SpawnPrefab;
        internal static delegate* unmanaged[Cdecl]<NativeString, uint> FindPersistent;
        internal static delegate* unmanaged[Cdecl]<NativeString, uint> EntityFind;
        internal static delegate* unmanaged[Cdecl]<uint, Math.Vector3*, uint, uint> EntityPosition;
        internal static delegate* unmanaged[Cdecl]<uint, uint> EntityDestroy;
        internal static delegate* unmanaged[Cdecl]<NativeString, void> SetHudText;
#pragma warning restore CS0649
    }
}
