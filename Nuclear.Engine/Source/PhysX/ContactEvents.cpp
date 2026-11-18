#include "ContactEvents.h"

namespace Nuclear::PhysX
{
    using namespace physx;
    static std::uint32_t EntityId(PxActor* actor)
    {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(actor->userData));
    }

    void ContactEvents::onContact(const PxContactPairHeader& header, const PxContactPair* pairs, PxU32 count)
    {
        if (header.flags & (PxContactPairHeaderFlag::eREMOVED_ACTOR_0 | PxContactPairHeaderFlag::eREMOVED_ACTOR_1)) return;
        for (PxU32 i = 0; i < count; ++i)
        {
            const auto events = pairs[i].events;
            if (events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
                Pending.emplace_back(EntityId(header.actors[0]), EntityId(header.actors[1]), false, true);
            if (events & PxPairFlag::eNOTIFY_TOUCH_LOST)
                Pending.emplace_back(EntityId(header.actors[0]), EntityId(header.actors[1]), false, false);
        }
    }

    void ContactEvents::onTrigger(PxTriggerPair* pairs, PxU32 count)
    {
        for (PxU32 i = 0; i < count; ++i)
        {
            const auto& pair = pairs[i];
            if (pair.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER)) continue;
            Pending.emplace_back(EntityId(pair.triggerActor), EntityId(pair.otherActor), true,
                pair.status == PxPairFlag::eNOTIFY_TOUCH_FOUND);
        }
    }
}
