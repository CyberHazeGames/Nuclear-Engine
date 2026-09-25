#pragma once
#include <NE_Common.h>
#include <Core/EngineModule.h>

namespace physx {
	class PxFoundation;
	class PxPhysics;
	class PxPvd;
	class PxCpuDispatcher;
	class PxMaterial;
	class PxDefaultCpuDispatcher;
}
namespace Nuclear
{
	namespace Components {
		class ColliderComponent;
		class RigidBodyComponent;
	}
	namespace PhysX
	{
		using namespace physx;
		struct PhysXModuleDesc
		{

		};

		class NEAPI PhysXModule : public Core::EngineModule
		{
		public:	
			static PhysXModule& Get();
			bool OnInitialize() override;

			bool Initialize(const PhysXModuleDesc& desc);
			void Shutdown() override;

			PxFoundation* GetFoundation();
			PxPhysics* GetPhysics();
			PxPvd* GetPvd();
			PxCpuDispatcher* GetCPUDispatcher();

			PxMaterial& GetDefaultMaterial();

		protected:
			PxFoundation* gFoundation = NULL;
			PxPhysics* gPhysics = NULL;
			PxPvd* gPvd = NULL;
			PxDefaultCpuDispatcher* gDispatcher = NULL;
			PxMaterial* gDefaultMaterial;

		private:
			PhysXModule();
		};
	}
}
