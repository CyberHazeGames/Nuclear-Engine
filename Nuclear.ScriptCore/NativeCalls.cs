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
#pragma warning restore CS0649
    }
}
