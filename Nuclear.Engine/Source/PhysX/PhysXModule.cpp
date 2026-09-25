#include "PhysX\PhysXModule.h"
#include "Components\ColliderComponent.h"
#include <PhysX/include/PxPhysicsAPI.h>
#include <Utilities/Logger.h>


#pragma comment(lib,"PhysX_64.lib")
#pragma comment(lib,"PhysXCommon_64.lib")
#pragma comment(lib,"PhysXFoundation_64.lib")
#pragma comment(lib,"PhysXExtensions_static_64.lib")
#pragma comment(lib,"PhysXPvdSDK_static_64.lib")

#define PX_RELEASE(x) if(x) { x->release(); }
using namespace physx;

namespace Nuclear
{
	namespace PhysX
	{
		class NuclearPXErrorCallback : public PxErrorCallback
		{
		public:
			virtual void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override
			{
				NUCLEAR_ERROR("[PhysX] Error Code: '{0}' Details: '{1}'", code , message);
			}
		}gErrorCallback;

		PhysXModule& PhysXModule::Get() { static PhysXModule instance; return instance; }
		bool PhysXModule::OnInitialize() { return Initialize({}); }
		PhysXModule::PhysXModule()
		{

		}

		PxDefaultAllocator gAllocator;

		bool PhysXModule::Initialize(const PhysXModuleDesc& desc)
		{
			gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
			
			gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true);

			if (gPhysics == NULL)
			{
				NUCLEAR_ERROR("[PhysXModule] PxCreatePhysics Failed!");
				return false;
			}

			PxInitExtensions(*gPhysics, nullptr);

			PxU32 numCores = 4;
			gDispatcher = PxDefaultCpuDispatcherCreate(numCores == 0 ? 0 : numCores - 1);

			if (!gDispatcher)
			{
				NUCLEAR_ERROR("[PhysXModule] PxDefaultCpuDispatcherCreate Failed!");
				return false;
			}

			// setup default material...
			gDefaultMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.1f);
			if (!gDefaultMaterial)
			{
				NUCLEAR_ERROR("[PhysXModule] createMaterial(gDefaultMaterial) Failed!");
				return false;
			}

			NUCLEAR_INFO("[PhysXModule] PhysX has been initialized succesfully!");
			return true;
		}
		void PhysXModule::Shutdown()
		{
			NUCLEAR_INFO("[PhysXModule] Shutting down...");

			PX_RELEASE(gDefaultMaterial)
			PX_RELEASE(gPvd)
			PX_RELEASE(gDispatcher)
			PX_RELEASE(gPhysics)
			PX_RELEASE(gFoundation)	
		}
		PxFoundation* PhysXModule::GetFoundation()
		{
			return gFoundation;
		}
		PxPhysics* PhysXModule::GetPhysics()
		{
			return gPhysics;
		}
		PxPvd* PhysXModule::GetPvd()
		{
			return gPvd;
		}
		PxCpuDispatcher* PhysXModule::GetCPUDispatcher()
		{
			return gDispatcher;
		}
		PxMaterial& PhysXModule::GetDefaultMaterial()
		{
			return *gDefaultMaterial;
		}
	}
}
