#pragma once
#include <Assets/Mesh.h>
#include <Assets/Material.h>
#include <Assets/Texture.h>
#include <Diligent/Common/interface/RefCntAutoPtr.hpp>
#include <Diligent/Graphics/GraphicsEngine/interface/Buffer.h>
#include <Diligent/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Diligent/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Texture.h>
#include <memory>
#include <unordered_map>

namespace Nuclear::Editor
{
	/// Manages offscreen GPU rendering and caching of 3D mesh preview thumbnails.
	/// Creates an offscreen 128x128 RGBA8 render target with a 32-bit depth buffer, renders
	/// directional Blinn-Phong shaded thumbnails for created meshes, and caches the resulting
	/// Assets::Texture wrappers for display in UI::AssetTile widgets.
	class MeshPreviewCache
	{
	public:
		MeshPreviewCache();
		~MeshPreviewCache();

		/// Returns a cached preview texture for the specified mesh, rendering it on demand if needed.
		///
		/// @param mesh Pointer to the target mesh asset. Must be non-null and in Assets::IAsset::State::Created state.
		/// @return Borrowed pointer to the cached preview Assets::Texture object whose GetTextureView()
		///         can be sampled by UI::Image and UI::Brush, or nullptr if rendering fails or the mesh is not ready.
		///         Lifetime of the returned pointer is owned by this cache instance until Clear() or cache destruction.
		Assets::Texture* GetPreview(Assets::Mesh* mesh);

		/// Returns a cached studio-lit sphere using a private copy of the material's static forward shader and first texture set.
		/// Null/unready assets return null. The cache owns the returned texture until Clear().
		Assets::Texture* GetMaterialPreview(Assets::Material* material);

		/// Clears all cached preview textures and releases allocated GPU render targets and pipeline resources.
		void Clear();

	private:
		/// Compiles shaders and creates the offscreen graphics pipeline, constant buffer, and depth target.
		/// @return True if the rendering pipeline and resources are successfully initialized and ready for draw calls.
		bool InitializePipeline();
		/// Renders ready geometry into a new neutral cache-owned thumbnail.
		std::unique_ptr<Assets::Texture> RenderPreview(Assets::Mesh& mesh);

		/// Uniform buffer layout matching the mesh preview vertex and pixel shaders.
		struct Constants
		{
			/// Combined Model-View-Projection matrix for offscreen thumbnail rendering.
			Math::Matrix4 ModelViewProjection = Math::Matrix4(1);

			/// Normalized world-space direction vector pointing towards the thumbnail light source.
			Math::Vector4 LightDirection = Math::Vector4(0.577f, 0.577f, 0.577f, 0.0f);

			/// Diffuse base color tint applied to the mesh geometry.
			Math::Vector4 BaseColor = Math::Vector4(0.72f, 0.78f, 0.86f, 1.0f);
		};

		/// Diligent graphics pipeline state with depth testing and back-face culling for thumbnail rendering.
		Diligent::RefCntAutoPtr<Diligent::IPipelineState> mPipeline;

		/// Shader resource binding holding static uniform buffer bindings.
		Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> mBinding;

		/// Dynamic uniform buffer updated with per-draw camera matrices and lighting parameters.
		Diligent::RefCntAutoPtr<Diligent::IBuffer> mConstantBuffer;

		/// Reusable 128x128 32-bit float depth-stencil target for offscreen thumbnail rendering.
		Diligent::RefCntAutoPtr<Diligent::ITexture> mDepthTarget;

		/// Map from source mesh asset pointers to their cached preview texture wrappers.
		std::unordered_map<Assets::Mesh*, std::unique_ptr<Assets::Texture>> mCachedTextures;
		/// Library material identities mapped to independent sphere thumbnails.
		std::unordered_map<Assets::Material*, std::unique_ptr<Assets::Texture>> mMaterialTextures;

		/// Flag indicating whether pipeline creation has been attempted.
		bool mInitialized = false;
	};
} // namespace Nuclear::Editor
