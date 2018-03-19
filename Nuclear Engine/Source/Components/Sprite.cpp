#include "Components\Sprite.h"

namespace NuclearEngine
{
	namespace Components
	{
		Sprite::Sprite(Graphics::API::Texture * texture, Math::Vector2 position, Math::Vector2 size, Graphics::Color color, Math::Vector2 velocity)
		{
			Position = position;
			Size = size;
			Color = color;
			Velocity = velocity;
			Texture = texture;
			Rotation = 0.0f;
			IsSolid = true;
			Destroyed = false;
			
		}
	}
}