#pragma once
#include <PhysX/include/PxPhysicsAPI.h>
#include <vector>
#include <tuple>
#include <cstdint>

namespace Nuclear::PhysX
{
    /// Buffers actor IDs during PhysX callbacks so gameplay runs after fetchResults.
    class ContactEvents final : public physx::PxSimulationEventCallback
    {
    public:
        /// First actor, second actor, trigger flag and entering flag, in delivery order.
        std::vector<std::tuple<std::uint32_t, std::uint32_t, bool, bool>> Pending;
        void onContact(const physx::PxContactPairHeader&, const physx::PxContactPair*, physx::PxU32) override;
        void onTrigger(physx::PxTriggerPair*, physx::PxU32) override;
        void onWake(physx::PxActor**, physx::PxU32) override {}
        void onSleep(physx::PxActor**, physx::PxU32) override {}
        void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) override {}
        void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, physx::PxU32) override {}
    };
}
