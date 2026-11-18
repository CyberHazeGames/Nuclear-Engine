using System;
using Nuclear.Math;
using Nuclear.Managed.Interop;

namespace Nuclear.ECS
{
    public class Entity
    {
        public uint ID { get; private set; }
        protected Entity() { }

        // Called by the host after construction and before OnStart.
        internal unsafe void BindEntity(uint id)
        {
            ID = id;
            using NativeString result = NativeCalls.ScriptFields(id);
            string json = result.ToString();
            if (string.IsNullOrWhiteSpace(json)) return;
            using var fields = System.Text.Json.JsonDocument.Parse(json);
            var options = new System.Text.Json.JsonSerializerOptions { IncludeFields = true };
            foreach (var property in fields.RootElement.EnumerateObject())
            {
                var field = GetType().GetField(property.Name, System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public);
                if (field == null || field.IsInitOnly) throw new ArgumentException("Unknown editable script field: " + property.Name);
                field.SetValue(this, System.Text.Json.JsonSerializer.Deserialize(property.Value.GetRawText(), field.FieldType, options));
            }
        }

        public unsafe bool PlayAnimation(int clip, float blendSeconds = .2f) => NativeCalls.PlayClip(ID, clip, blendSeconds) != 0;
        public unsafe bool SetVelocity(Vector3 velocity) => NativeCalls.SetVelocity(ID, &velocity) != 0;
        public unsafe bool PlayAudio() => NativeCalls.AudioControl(ID, 0) != 0;
        public unsafe bool PauseAudio() => NativeCalls.AudioControl(ID, 1) != 0;
        public unsafe bool StopAudio() => NativeCalls.AudioControl(ID, 2) != 0;

        /// <summary>Finds the closest collider along a world-space ray, up to the given distance.</summary>
        public static unsafe Entity Raycast(Vector3 origin, Vector3 direction, float distance,
            out Vector3 position, out Vector3 normal, out float hitDistance)
        {
            float* hit = stackalloc float[7];
            uint id = NativeCalls.Raycast(&origin, &direction, distance, hit);
            position = normal = default;
            hitDistance = 0;
            if (id == uint.MaxValue) return null;
            position = new Vector3(hit[0], hit[1], hit[2]);
            normal = new Vector3(hit[3], hit[4], hit[5]);
            hitDistance = hit[6];
            return new Entity { ID = id };
        }

        /// <summary>Local position in world units. Accessing a removed entity throws.</summary>
        public unsafe Vector3 Position
        {
            get
            {
                Vector3 value;
                if (NativeCalls.EntityPosition(ID, &value, 0) == 0) throw new InvalidOperationException("Entity has no transform.");
                return value;
            }
            set
            {
                if (NativeCalls.EntityPosition(ID, &value, 1) == 0) throw new InvalidOperationException("Invalid entity or position.");
            }
        }

        /// <summary>Finds the first live entity with this exact name, or returns null.</summary>
        public static unsafe Entity Find(string name)
        {
            using NativeString text = name ?? string.Empty;
            uint id = NativeCalls.EntityFind(text);
            return id == uint.MaxValue ? null : new Entity { ID = id };
        }

        /// <summary>Queues removal after script updates. Returns false for the main camera or a repeated request.</summary>
        public unsafe bool Destroy() => NativeCalls.EntityDestroy(ID) != 0;

        /// <summary>Replaces the viewport's gameplay message. An empty string clears the text.</summary>
        public static unsafe void ShowHud(string message)
        {
            using NativeString text = message ?? string.Empty;
            NativeCalls.SetHudText(text);
        }

        public unsafe bool SetPaused(bool value) => NativeCalls.SetPaused(ID, value ? 1u : 0u) != 0;

        public static unsafe Entity Spawn(string prefab, Vector3 position)
        {
            using NativeString name = prefab;
            uint id = NativeCalls.SpawnPrefab(name, &position);
            return id == uint.MaxValue ? null : new Entity { ID = id };
        }

        public static unsafe Entity FindById(string identity)
        {
            using NativeString key = identity;
            uint id = NativeCalls.FindPersistent(key);
            return id == uint.MaxValue ? null : new Entity { ID = id };
        }

        public virtual void OnTriggerEnter(Entity other) { }
        public virtual void OnTriggerExit(Entity other) { }
        public virtual void OnCollisionEnter(Entity other) { }
        public virtual void OnCollisionExit(Entity other) { }

        public void TriggerEnter(uint id) => OnTriggerEnter(new Entity { ID = id });
        public void TriggerExit(uint id) => OnTriggerExit(new Entity { ID = id });
        public void CollisionEnter(uint id) => OnCollisionEnter(new Entity { ID = id });
        public void CollisionExit(uint id) => OnCollisionExit(new Entity { ID = id });

        public virtual void OnStart() { }
        public virtual void OnUpdate(float deltaTime) { }

        public unsafe T AddComponent<T>() where T : Components.Component, new()
        {
            if (NativeCalls.AddComponent(ID, typeof(T)) == 0)
                throw new InvalidOperationException($"Cannot add {typeof(T).FullName} to entity {ID}.");
            return new T { Entity = this };
        }

        public unsafe bool HasComponent<T>() where T : Components.Component, new()
        {
            return NativeCalls.HasComponent(ID, typeof(T)) != 0;
        }

        public T GetComponent<T>() where T : Components.Component, new()
        {
            return HasComponent<T>() ? new T { Entity = this } : null;
        }
    }
}
