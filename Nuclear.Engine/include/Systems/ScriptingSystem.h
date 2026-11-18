#pragma once
#include "Scripting\ScriptingModule.h"
#include "ECS\System.h"
#include <vector>

namespace Nuclear
{			
	namespace Systems
	{
		class NEAPI ScriptingSystem : public ECS::System<ScriptingSystem>
		{
		public:
			ScriptingSystem();
			~ScriptingSystem();

			bool Initialize();

			void Load() override;
			void Update(ECS::TimeDelta dt) override;
			/// Defers a live entity's destruction until every script finishes its update. Protects the main camera.
			static bool RequestDestroy(Uint32 id);
		private:
			std::vector<Uint32> mPendingDestroy; ///< Versioned entity IDs requested by this system's scripts.
			static ScriptingSystem* pActive; ///< Borrowed while this system is connected to the active registry.
		};

	}
}