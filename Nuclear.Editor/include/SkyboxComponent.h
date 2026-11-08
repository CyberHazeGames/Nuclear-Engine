#pragma once

namespace Nuclear::Editor
{
	/// Editor authoring marker for the scene's generated sky background; holds no GPU resources.
	class SkyboxComponent
	{
	public:
		SkyboxComponent(); ///< Starts with the sky visible.
		bool Enabled = true; ///< Whether the sky replaces clear-color pixels behind scene geometry.
	};
}
