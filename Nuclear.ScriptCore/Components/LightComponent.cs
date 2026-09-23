namespace Nuclear.Components
{
    public class LightComponent : Component
    {
        public unsafe Graphics.Color Color
        {
            get
            {
                Graphics.Color result;
                NativeCalls.LightGetColor(Entity.ID, &result);
                return result;
            }
            set { NativeCalls.LightSetColor(Entity.ID, &value); }
        }

        public unsafe float Intensity
        {
            get { return NativeCalls.LightGetIntensity(Entity.ID); }
            set { NativeCalls.LightSetIntensity(Entity.ID, value); }
        }
    }
}
