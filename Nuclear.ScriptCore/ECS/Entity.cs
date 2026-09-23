using System;

namespace Nuclear.ECS
{
    public class Entity
    {
        public uint ID { get; private set; }
        protected Entity() { }

        // Called by the host after construction and before OnStart.
        internal void BindEntity(uint id) { ID = id; }

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
