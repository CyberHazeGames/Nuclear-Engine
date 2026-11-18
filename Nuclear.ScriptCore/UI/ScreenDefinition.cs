using System;

namespace Nuclear.UI
{
    /// <summary>Serializable menu content. Set this public field in the script inspector using Title and Buttons.</summary>
    public sealed class ScreenDefinition
    {
        public string Title = "";
        /// <summary>Each pair is a visible caption and a game-defined command.</summary>
        public string[][] Buttons = Array.Empty<string[]>();

        /// <summary>Builds keyboard-focusable native buttons and dispatches each authored command on activation.</summary>
        public GameScreen Open(Action<string> run, Func<string, bool> available = null)
        {
            ArgumentNullException.ThrowIfNull(run);
            var screen = new GameScreen(Title);
            foreach (var button in Buttons)
            {
                if (button == null || button.Length != 2 || string.IsNullOrWhiteSpace(button[0]) || string.IsNullOrWhiteSpace(button[1]))
                    throw new ArgumentException("Each screen button requires a caption and a command.");
                string command = button[1];
                if (available == null || available(command)) screen.Button(button[0], () => run(command));
            }
            return screen;
        }
    }
}
