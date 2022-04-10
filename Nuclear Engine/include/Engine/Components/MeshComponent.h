#pragma once
#include <Core\NE_Common.h>
#include <Core\Math\Math.h>
#include <vector>

namespace NuclearEngine {
	namespace Assets 
	{
		class Mesh;
		class Material;
	}
	namespace Components 
	{

		class NEAPI MeshComponent
		{
		public:			
			MeshComponent();
			MeshComponent(Assets::Mesh *mesh , bool MultiRender = false);
			MeshComponent(Assets::Mesh *mesh , Assets::Material * material, bool MultiRender = false);
			~MeshComponent();

			//if false the RenderSystem won't render the model
			bool mRender = true;

			bool mMultiRender = false;
			std::vector<Math::Matrix4> mMultiRenderTransforms;

			Assets::Mesh * mMesh;
			Assets::Material * mMaterial;

		};

	}
}