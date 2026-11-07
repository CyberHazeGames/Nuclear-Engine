#pragma once

#include <Core/EngineModule.h>
#include <UI/UIContext.h>
#include <UI/UIRenderer.h>

namespace Nuclear::UI
{
	/// Runtime viewport UI service; depends on Graphics and Assets, never on ImGui or ECS.
	class NEAPI UIModule : public Core::EngineModule
	{
	public:
		static UIModule& Get(); ///< Shared engine service.
		~UIModule();
		bool OnInitialize() override; ///< Creates renderer and a private copy of the default font.
		void Shutdown() override; ///< Clears widgets/resources before graphics teardown.
		/// Viewport context, valid for module lifetime.
		UIContext& GetContext()
		{
			return mContext;
		}
		/// True after successful GPU initialization.
		bool IsInitialized() const
		{
			return mInitialized;
		}
		/// Draws after client rendering; the engine synchronizes dimensions through resize events.
		void Render();

	private:
		UIModule() = default;
		UIContext mContext; ///< Owns client widgets and input state.
		UIRenderer mRenderer; ///< Dedicated Diligent renderer.
		std::unique_ptr<Assets::Font> pDefaultFont; ///< Private font copy survives asset-library flushes.
		bool mInitialized = false; ///< Guards optional module use and repeated shutdown.
	};
} // namespace Nuclear::UI
