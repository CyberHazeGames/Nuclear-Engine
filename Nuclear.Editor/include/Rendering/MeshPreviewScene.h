#pragma once
#include <Assets/Mesh.h>
#include <Assets/Texture.h>
#include <Components/CameraComponent.h>
#include <EditorSkybox.h>
#include <Diligent/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Diligent/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <memory>

namespace Nuclear::Editor
{
	class MaterialPreviewRenderer;
	/// Private lit mesh/sky environment; never registers entities or changes the active scene camera.
	class MeshPreviewScene
	{
	public:
		MeshPreviewScene(); ///< Defers GPU allocation until the first visible render.
		~MeshPreviewScene(); ///< Releases private material shaders before graphics shutdown.
		/// Borrows a ready mesh for this call and renders at physical-pixel dimensions; false on failure.
		/// An optional material uses its private forward shader and selected texture set instead of neutral shading.
		bool Render(Assets::Mesh& mesh, Uint32 width, Uint32 height, Assets::Material* material = nullptr, Uint32 textureSet = 0);
		void FrameMesh(); ///< Resets the camera to show the measured mesh bounds, preserving asset geometry.
		Components::CameraComponent& GetCamera() { return mCamera; } ///< Private document camera, valid until destruction.
		Assets::Texture& GetTexture() { return mTexture; } ///< Borrowed output wrapper, valid until destruction.
	private:
		bool InitializePipeline(); ///< Creates the neutral lit geometry pipeline; false if GPU allocation fails.
		void MeasureMesh(Assets::Mesh& mesh); ///< Reads retained CPU vertices or staging GPU buffers once to fit uploaded geometry.
		/// Uniform layout shared by the private vertex and pixel shaders.
		struct Constants
		{
			Math::Matrix4 ModelViewProjection; ///< Column-major model/view/projection transform.
			Math::Vector4 LightDirection; ///< Unit world-space direction toward the demo light.
			Math::Vector4 BaseColor; ///< Neutral linear diffuse color.
		};
		Components::CameraComponent mCamera; ///< Owned camera and offscreen color/depth targets.
		EditorSkybox mSkybox; ///< Owned default generated sky; no imported asset dependency.
		Assets::Texture mTexture; ///< Output view borrowed by the retained preview surface.
		Diligent::RefCntAutoPtr<Diligent::IPipelineState> mPipeline; ///< Lit mesh pipeline.
		Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> mBinding; ///< Private geometry bindings.
		Diligent::RefCntAutoPtr<Diligent::IBuffer> mConstantBuffer; ///< Per-frame geometry transform and light.
		Math::Vector3 mCenter{0}; ///< Finite mesh bounds center in asset units.
		float mRadius = 1; ///< Positive bounding-sphere radius in asset units.
		bool mInitialized = false; ///< Pipeline and sky are ready.
		bool mMeasured = false; ///< Initial bounds and camera framing have been computed.
		std::unique_ptr<MaterialPreviewRenderer> pMaterialRenderer; ///< Private material pipeline; never rebakes a library shader.
	};
}
