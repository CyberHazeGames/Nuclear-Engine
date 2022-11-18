#pragma once
#include <Engine/Rendering/RenderingPath.h>

namespace Nuclear
{
	namespace Rendering
	{
		class NEAPI ForwardRenderingPath : public RenderingPath
		{
		public:
			void StartRendering(Graphics::ShaderPipelineVariant* pipeline) override;

			void Render(Components::MeshComponent& mesh, const Math::Matrix4& modelmatrix) override;
		};
	}
}