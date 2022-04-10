#pragma once
#include <Core\NE_Common.h>
#include <Engine\Components\MeshComponent.h>
#include <Engine\Components\CameraComponent.h>
#include <Engine\Assets\Mesh.h>
#include <Engine\Assets\Material.h>
#include <Diligent/Common/interface/RefCntAutoPtr.hpp>
#include <Diligent/Graphics/GraphicsEngine/interface/Shader.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Buffer.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Texture.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Sampler.h>
#include <Diligent/Graphics/GraphicsEngine/interface/DepthStencilState.h>
#include <Engine\Graphics\RenderingPipelines\RenderingPipeline.h>
#include <vector>

namespace NuclearEngine
{
	namespace Graphics
	{

		class NEAPI DiffuseOnly : public RenderingPipeline
		{
		public:
			DiffuseOnly();

			bool Bake(const RenderingPipelineDesc& desc) override;

		private:

		};

	}
}