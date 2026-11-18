#include "Components/EntityInfoComponent.h"

#include <Core/UUID.h>
namespace Nuclear
{
	namespace Components
	{
		EntityInfoComponent::EntityInfoComponent(const ECS::Transform &Transform, const std::string& name)
			: mName(name), mTransform(Transform), PersistentId(Core::UUID::CreateNewUUID().str())
		{
		}

		EntityInfoComponent::~EntityInfoComponent()
		{
		}
	}
}