#pragma once
#include <Common.h>

namespace Nuclear::Editor
{
	/// Session-only scene properties, separate from entity snapshots and disk scene serialization.
	class EditorSceneState
	{
	public:
		static EditorSceneState Capture(); ///< Reads the active scene name and preview camera clear color.
		bool IsValid() const; ///< True when captured from a scene with a preview camera.
		bool Restore() const; ///< Restores this scene's settings using the current camera; false after scene replacement.
		bool operator==(const EditorSceneState&) const = default; ///< Compares scene identity and authored settings.
		std::string Name; ///< Scene asset display name, owned by the snapshot.
		Math::Vector4 ClearColor{.025f, .04f, .055f, 1.f}; ///< Linear RGBA values used to clear the preview target.
	private:
		Assets::Scene* pScene = nullptr; ///< Borrowed session asset identity; never owns or retains a camera pointer.
	};
}
