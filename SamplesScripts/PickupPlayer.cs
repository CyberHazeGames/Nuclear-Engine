using Nuclear.ECS;
using Nuclear.Math;
using Nuclear.Platform;

namespace Samples
{
    /// <summary>A WASD player that collects three named pickups within one world unit.</summary>
    public class PickupPlayer : Entity
    {
        private int collected;
        public override void OnStart() => ShowHud("0/3   Move with WASD");

        public override void OnUpdate(float seconds)
        {
            if (collected == 3) return;
            float x = (Input.IsKeyPressed(Input.KeyCode.D) ? 1 : 0) - (Input.IsKeyPressed(Input.KeyCode.A) ? 1 : 0);
            float z = (Input.IsKeyPressed(Input.KeyCode.S) ? 1 : 0) - (Input.IsKeyPressed(Input.KeyCode.W) ? 1 : 0);
            float length = System.MathF.Sqrt(x * x + z * z);
            if (length > 0) Position += new Vector3(x, 0, z) * (4 * seconds / length);

            for (int i = 1; i <= 3; ++i)
            {
                var pickup = Find("Pickup " + i);
                if (pickup == null) continue;
                var d = pickup.Position - Position;
                if (d.x * d.x + d.y * d.y + d.z * d.z <= 1 && pickup.Destroy())
                {
                    ++collected;
                    ShowHud(collected == 3 ? "3/3   You collected them all!" : collected + "/3");
                }
            }
        }
    }
}
