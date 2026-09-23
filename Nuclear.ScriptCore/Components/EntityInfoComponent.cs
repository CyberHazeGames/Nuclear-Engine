using Nuclear.Managed.Interop;

namespace Nuclear.Components
{
    public class EntityInfoComponent : Component
    {
        public string Name
        {
            get => GetName_Native(Entity.ID);
            set => SetName_Native(Entity.ID, value);
        }

        public static unsafe string GetName_Native(uint entity)
        {
            using NativeString result = NativeCalls.EntityGetName(entity);
            return result.ToString() ?? string.Empty;
        }

        public static unsafe void SetName_Native(uint entity, string name)
        {
            using NativeString text = name ?? string.Empty;
            NativeCalls.EntitySetName(entity, text);
        }
    }
}
