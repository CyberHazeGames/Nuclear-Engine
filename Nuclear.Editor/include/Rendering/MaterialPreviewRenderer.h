#pragma once
#include <Assets/Shader.h>
#include <Assets/Material.h>
#include <Assets/Mesh.h>
#include <Components/CameraComponent.h>
#include <memory>

namespace Nuclear::Editor
{
	/// Owns a private static forward shader, material bindings and studio lights for a preview sphere.
	/// Shader inheritance provides access to a copied build description; library shaders are never rebaked.
	class MaterialPreviewRenderer : private Assets::Shader
	{
	public:
		/// Draws a ready mesh with one selected source texture set into the supplied private camera targets.
		/// Inputs are borrowed for this call; returns false for unready or unsupported forward shaders.
		bool Render(Assets::Mesh& mesh, Components::CameraComponent& camera, Assets::Material& material, Uint32 textureSet);
	private:
		/// Copies source shader metadata and bakes one forward variant against private camera/light buffers.
		bool Initialize(Assets::Shader& source, Components::CameraComponent& camera);
		/// Copies the selected source set, supplies missing standard maps and creates private texture bindings.
		void PrepareMaterial(Assets::Material& source, Uint32 textureSet);
		Assets::Shader* pSourceShader = nullptr; ///< Borrowed identity used to detect a changed source shader.
		Assets::Material* pSourceMaterial = nullptr; ///< Borrowed identity for the current copied texture set.
		Uint32 mTextureSet = 0; ///< Source set represented by the current sphere bindings.
		std::unique_ptr<Assets::Material> pMaterial; ///< Private bindings; source textures remain library-owned.
		Diligent::RefCntAutoPtr<Diligent::IBuffer> mCameraBuffer; ///< Private model/view/projection constants.
		Diligent::RefCntAutoPtr<Diligent::IBuffer> mLightBuffer; ///< Camera position followed by two directional studio lights.
	};
}
