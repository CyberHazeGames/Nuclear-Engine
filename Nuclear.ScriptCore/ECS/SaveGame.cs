using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace Nuclear.ECS
{
    /// <summary>Per-user JSON save slots scoped by project name. Successful saves replace an entire slot atomically.</summary>
    public static class SaveGame
    {
        private static string SlotPath(string slot)
        {
            ArgumentException.ThrowIfNullOrWhiteSpace(slot);
            string game = GameScene.Game;
            if (game.Length == 0) throw new InvalidOperationException("A running project is required.");
            static string Key(string text) => Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(text)));
            string root = Environment.GetEnvironmentVariable("NUCLEAR_SAVE_ROOT");
            if (string.IsNullOrWhiteSpace(root))
                root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Nuclear", "Games");
            return Path.Combine(root, Key(game), Key(slot) + ".json");
        }

        public static void Write<T>(string slot, T state)
        {
            string path = SlotPath(slot);
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            string temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                {
                    JsonSerializer.Serialize(stream, state);
                    stream.Flush(true);
                }
                File.Move(temporary, path, true);
            }
            finally { File.Delete(temporary); }
        }

        /// <summary>Returns false for an unused slot. Malformed or inaccessible saves raise an exception.</summary>
        public static bool TryRead<T>(string slot, out T state)
        {
            try
            {
                using var file = new FileStream(SlotPath(slot), FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete);
                state = JsonSerializer.Deserialize<T>(file);
                return true;
            }
            catch (FileNotFoundException) { state = default; return false; }
            catch (DirectoryNotFoundException) { state = default; return false; }
        }
    }
}
