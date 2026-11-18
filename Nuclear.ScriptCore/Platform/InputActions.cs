using System;
using System.Collections.Generic;
using Nuclear.ECS;

namespace Nuclear.Platform
{
    /// <summary>A game-defined set of named actions with persistent, per-player keyboard and gamepad bindings.</summary>
    public sealed class InputActions
    {
        public readonly record struct Binding(Input.KeyCode Key, GamepadButton Button);
        private readonly Dictionary<string, Binding> bindings = new(StringComparer.Ordinal);

        public void Bind(string action, Input.KeyCode key, GamepadButton button = GamepadButton.None)
        {
            ArgumentException.ThrowIfNullOrWhiteSpace(action);
            bindings[action] = new(key, button);
        }

        public bool IsHeld(string action, uint controller = 0)
        {
            if (!bindings.TryGetValue(action, out var binding)) return false;
            return Input.IsKeyPressed(binding.Key) || (binding.Button != 0 && Gamepad.TryRead(controller, out var pad)
                && (pad.Buttons & binding.Button) == binding.Button);
        }

        public void Save(string profile = "controls") => SaveGame.Write(profile, bindings);

        /// <summary>Applies overrides for actions currently defined by the game.</summary>
        public void Restore(string profile = "controls")
        {
            if (SaveGame.TryRead<Dictionary<string, Binding>>(profile, out var saved) && saved != null)
                foreach (var pair in saved)
                    if (bindings.ContainsKey(pair.Key) && Enum.IsDefined(pair.Value.Key))
                        bindings[pair.Key] = pair.Value;
        }
    }
}
