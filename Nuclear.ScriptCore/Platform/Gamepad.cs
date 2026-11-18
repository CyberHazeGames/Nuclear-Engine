using System;
using System.Runtime.InteropServices;

namespace Nuclear.Platform
{
    [Flags]
    public enum GamepadButton : uint
    {
        None = 0, Up = 1, Down = 2, Left = 4, Right = 8, Start = 16, Back = 32,
        LeftStick = 64, RightStick = 128, LeftBumper = 256, RightBumper = 512,
        A = 4096, B = 8192, X = 16384, Y = 32768
    }

    /// <summary>Digital controls and normalized analog axes for one connected controller.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Gamepad
    {
        public GamepadButton Buttons;
        public float LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger;

        public static unsafe bool TryRead(uint slot, out Gamepad state)
        {
            Gamepad value = default;
            bool connected = NativeCalls.ReadGamepad(slot, &value) != 0;
            state = value;
            return connected;
        }
    }
}
