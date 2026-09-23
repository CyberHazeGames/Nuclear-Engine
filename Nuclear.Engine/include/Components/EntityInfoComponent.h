#pragma once
#include <ECS/Transform.h>
#include <ECS/Entity.h>
#include <string>
namespace Nuclear
{
	namespace Components
	{
		class NEAPI EntityInfoComponent
		{
		public:
			EntityInfoComponent(const ECS::Transform& Transform = ECS::Transform(), const std::string& name = "Entity");
			~EntityInfoComponent();

			ECS::Transform mTransform;
			std::string mName;

		//private:
		//	friend Serialization::Access;

			constexpr static auto serialize(auto& archive, auto& self)
			{
				return archive(self.mName, self.mTransform);
			}
		};
	}
}