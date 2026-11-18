using Nuclear.Managed.Interop;

namespace Nuclear.ECS
{
    /// <summary>Scene names and transitions for the active project player.</summary>
    public static unsafe class GameScene
    {
        public static string Name => ReadName(true);
        public static string Game => ReadName(false);

        private static string ReadName(bool level)
        {
            using NativeString result = NativeCalls.GameName(level ? 1u : 0u);
            return result.ToString() ?? string.Empty;
        }

        /// <summary>Queues the first valid named transition for this frame.</summary>
        public static bool Load(string name)
        {
            using NativeString value = name ?? string.Empty;
            return NativeCalls.LoadLevel(value) != 0;
        }

        public static bool Reload() => Load(Name);
    }
}
