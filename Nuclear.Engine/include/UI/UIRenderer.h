#pragma once

#include <UI/DrawList.h>
#include <UI/Types.h>
#include <memory>

namespace Nuclear::UI
{
	/// Diligent renderer for ordered UI quads. Owns GPU resources independently of ImGui.
	class NEAPI UIRenderer
	{
	public:
		UIRenderer();
		~UIRenderer();
		UIRenderer(const UIRenderer&) = delete;
		UIRenderer& operator=(const UIRenderer&) = delete;
		/// Creates shaders/PSO for the current swap-chain format; requires initialized Graphics.
		bool Initialize();
		/// Releases GPU buffers, bindings, shaders, and fallback texture before Graphics shutdown.
		void Shutdown();
		/// Draws over the current swap-chain back buffer without clearing. Pixel size is physical;
		/// scale maps logical draw-list units to pixels. Rebinds all state it needs.
		/// target optionally supplies another back buffer with the initialized swap-chain color format.
		/// The borrowed target must remain live for this call; null uses the main swap-chain buffer.
		void Render(const DrawList& list, Vec2 pixelSize, float scale, Diligent::ITextureView* target = nullptr);

	private:
		struct Resources; ///< Private renderer objects keep Diligent out of the widget API.
		std::unique_ptr<Resources> pResources; ///< Exclusive GPU resource storage.
	};
} // namespace Nuclear::UI
