#pragma once
#include <Rendering/Skybox.h>
#include <Assets/Texture.h>

namespace Nuclear::Editor
{
	/// Owns a generated default cube texture and the engine skybox renderer. Destroy before graphics shutdown.
	class EditorSkybox
	{
	public:
		bool Initialize(Components::CameraComponent& camera); ///< Creates a seamless sky cube for the camera's target formats; false on GPU failure.
		void Render(Components::CameraComponent& camera); ///< Draws at far depth behind existing geometry; camera constants must already be current.
	private:
		Assets::Texture mTexture; ///< Privately owned generated cubemap, independent of the imported-asset library.
		Rendering::Skybox mSkybox; ///< Owns shared references to the cube view, pipeline and vertex buffer.
	};
}
