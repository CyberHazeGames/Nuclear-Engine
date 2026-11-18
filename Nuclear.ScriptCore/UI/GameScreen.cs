using System;
using System.Collections.Generic;
using Nuclear.Managed.Interop;

namespace Nuclear.UI
{
    /// <summary>A native, keyboard-focusable gameplay menu. Call Update from the owner's OnUpdate.</summary>
    public sealed unsafe class GameScreen
    {
        private readonly List<Action> commands = new();
        private readonly ulong id;

        public GameScreen(string title)
        {
            using NativeString text = title;
            id = NativeCalls.OpenScreen(text, 1);
        }

        public void Button(string caption, Action clicked)
        {
            ArgumentNullException.ThrowIfNull(clicked);
            using NativeString text = caption;
            if (NativeCalls.ScreenButton(id, text, checked((uint)commands.Count + 1)) == 0)
                throw new InvalidOperationException("This screen is no longer active.");
            commands.Add(clicked);
        }

        public void Update()
        {
            uint command;
            while ((command = NativeCalls.PollScreen(id)) != 0) commands[(int)command - 1]();
        }

        public static void Close()
        {
            using NativeString empty = string.Empty;
            NativeCalls.OpenScreen(empty, 0);
        }
    }
}
