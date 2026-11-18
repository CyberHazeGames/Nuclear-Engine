using Nuclear.ECS;
using Nuclear.Math;
using Nuclear.Platform;
using Nuclear.UI;

namespace Samples
{
    /// <summary>Two collectible rooms, a menu, editable movement controls and a persistent level checkpoint.</summary>
    public class RoomGame : Entity
    {
        public ScreenDefinition MainMenu = new() {
            Title = "Twin Rooms", Buttons = new[] { new[] { "New game", "start" }, new[] { "Continue", "continue" } }
        };
        public string NextLevel = "";
        public float Speed = 4;
        public int PickupCount = 3;
        private readonly InputActions actions = new();
        private GameScreen menu;
        private int remaining;
        private bool pauseWasHeld;
        private string rebind;
        private bool waitForRelease;

        public override void OnStart()
        {
            if (GameScene.Name == "Title")
            {
                bool canContinue = SaveGame.TryRead<string>("checkpoint", out var level) && (level == "Gallery" || level == "Atrium");
                menu = MainMenu.Open(command => {
                    if (command == "start") Enter("Gallery");
                    else if (command == "continue" && canContinue) Enter(level);
                }, command => command != "continue" || canContinue);
                return;
            }
            actions.Bind("forward", Input.KeyCode.W, GamepadButton.Up);
            actions.Bind("back", Input.KeyCode.S, GamepadButton.Down);
            actions.Bind("left", Input.KeyCode.A, GamepadButton.Left);
            actions.Bind("right", Input.KeyCode.D, GamepadButton.Right);
            actions.Bind("pause", Input.KeyCode.P, GamepadButton.Start);
            actions.Restore();
            remaining = PickupCount;
            for (int i = 0; i < remaining; ++i)
                Spawn("Coin", new Vector3(0, 1, 2 - 3 * i));
            ShowHud(GameScene.Name + "   Pickups left: " + remaining);
        }

        private void Enter(string level)
        {
            if (GameScene.Load(level)) SaveGame.Write("checkpoint", level);
        }

        public override void OnUpdate(float dt)
        {
            if (rebind != null)
            {
                Input.KeyCode pressed = Input.KeyCode.UNKNOWN;
                foreach (var key in System.Enum.GetValues<Input.KeyCode>())
                    if (key != Input.KeyCode.UNKNOWN && key != Input.KeyCode.ESCAPE && key != Input.KeyCode.NUM_SCANCODES && Input.IsKeyPressed(key))
                    { pressed = key; break; }
                if (waitForRelease) { waitForRelease = pressed != Input.KeyCode.UNKNOWN; return; }
                if (pressed == Input.KeyCode.UNKNOWN) return;
                var button = rebind switch { "forward" => GamepadButton.Up, "back" => GamepadButton.Down,
                    "left" => GamepadButton.Left, _ => GamepadButton.Right };
                actions.Bind(rebind, pressed, button);
                actions.Save();
                rebind = null;
                ShowPause();
                return;
            }
            bool pause = actions.IsHeld("pause");
            if (pause && !pauseWasHeld && menu == null)
            {
                ShowPause();
            }
            pauseWasHeld = pause;
            if (menu != null) { menu.Update(); return; }
            float x = (actions.IsHeld("right") ? 1 : 0) - (actions.IsHeld("left") ? 1 : 0);
            float z = (actions.IsHeld("back") ? 1 : 0) - (actions.IsHeld("forward") ? 1 : 0);
            float magnitude = System.MathF.Sqrt(x*x + z*z);
            if (magnitude > 0) Position += new Vector3(x, 0, z) * (Speed * dt / magnitude);
        }

        private void ShowPause()
        {
            menu = new GameScreen("Paused");
            menu.Button("Resume", () => { GameScreen.Close(); menu = null; SetPaused(false); });
            foreach (var action in new[] { "forward", "back", "left", "right" })
            {
                menu.Button("Rebind " + action, () => {
                    rebind = action;
                    waitForRelease = true;
                    menu = new GameScreen("Press a key for " + action);
                });
            }
            menu.Button("Title", () => GameScene.Load("Title"));
            SetPaused(true);
        }

        public override void OnTriggerEnter(Entity other)
        {
            if (remaining == 0 || other.GetComponent<Nuclear.Components.EntityInfoComponent>()?.Name != "Coin" || !other.Destroy()) return;
            --remaining;
            ShowHud(GameScene.Name + "   Pickups left: " + remaining);
            if (remaining != 0) return;
            if (NextLevel.Length > 0) { Enter(NextLevel); return; }
            SaveGame.Write("checkpoint", "");
            Nuclear.Utilities.Logger.Info("Twin Rooms completed");
            menu = new GameScreen("You collected every coin");
            menu.Button("Play again", () => Enter("Gallery"));
            menu.Button("Title", () => GameScene.Load("Title"));
        }
    }
}
