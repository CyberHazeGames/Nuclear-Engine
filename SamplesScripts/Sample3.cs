using System;
using Nuclear;
using Nuclear.ECS;
using Nuclear.Platform;
using Nuclear.Utilities;

namespace Samples
{
    public class Sample3 : Entity
    {
        public override void OnStart()
        {
            Logger.Info("Script Start id " + ID);
        }

        public override void OnUpdate(float td)
        {
            //Logger.Info("Update Time Delta "+ td);

            //bool result = HasComponent<Nuclear.Components.EntityInfoComponent>();
            //Logger.Info("Result " + result);

            if(Input.IsKeyPressed(Input.KeyCode.G))
            {
                Logger.Info("G KeyPressed ");
            }
        
        }
    }
}
