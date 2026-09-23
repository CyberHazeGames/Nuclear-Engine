using System;
using Nuclear.Components;
using Nuclear.ECS;
using Nuclear.Graphics;
using Nuclear.Platform;
using Nuclear.Utilities;

namespace ScriptingSmoke
{
    public class UnsupportedComponent : Component { }

    public class Probe : Entity
    {
        public uint StartedWithID;
        public float LastDelta;
        public int RuntimeMajor = Environment.Version.Major;
        public int BridgeIdentityMatches = typeof(Nuclear.Managed.Interop.NativeString).Assembly.GetName().Name == "Nuclear.Managed" ? 1 : 0;

        public override void OnStart()
        {
            StartedWithID = ID;
            if (ID != 42 || !HasComponent<EntityInfoComponent>())
                throw new Exception("Entity binding or component lookup failed.");
            var info = GetComponent<EntityInfoComponent>();
            info.Name = "Entity {42} — مرحبا";
            if (info.Name != "Entity {42} — مرحبا")
                throw new Exception("Entity name round trip failed.");
            var light = AddComponent<LightComponent>();
            light.Intensity = 3.25f;
            light.Color = new Color(0.2f, 0.4f, 0.6f, 0.8f);
            var color = light.Color;
            if (light.Intensity != 3.25f || color.r != 0.2f || color.g != 0.4f || color.b != 0.6f || color.a != 0.8f)
                throw new Exception("Light ABI round trip failed.");
            if (HasComponent<UnsupportedComponent>())
                throw new Exception("Unknown component was accepted.");
            try
            {
                AddComponent<UnsupportedComponent>();
                throw new Exception("Unsupported component did not fail.");
            }
            catch (InvalidOperationException) { }
            Logger.Trace("trace");
            Logger.Info("UTF-8 {braces} — مرحبا");
            Logger.Warn("warn");
            Logger.Error("error");
            Logger.Fatal("fatal");
        }

        public override void OnUpdate(float deltaTime)
        {
            LastDelta = deltaTime;
            if (!Input.IsKeyPressed(Input.KeyCode.G) || Input.IsKeyPressed(Input.KeyCode.H))
                throw new Exception("Input bool ABI failed.");
        }

        public void ThrowExpected() => throw new InvalidOperationException("Expected smoke exception");
    }
}
