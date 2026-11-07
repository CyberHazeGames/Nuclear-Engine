#include <Assets/AssetManager.h>
#include <Assets/Font.h>
#include <Core/Engine.h>
#include <Graphics/GraphicsModule.h>
#include <UI/UIModule.h>
#include <Utilities/Logger.h>

namespace Nuclear::UI
{
	UIModule& UIModule::Get()
	{
		static UIModule module;
		return module;
	}
	UIModule::~UIModule() = default;

	bool UIModule::OnInitialize()
	{
		if (!mRenderer.Initialize())
		{
			Shutdown();
			return false;
		}
		// The library owns imported assets. Copy the font's metrics and ref-counted GPU view
		// so client asset flushes cannot invalidate the module's default typeface.
		if (auto* font = Assets::AssetManager::Get().Import<Assets::Font>("@NuclearAssets@/Fonts/Roboto-Medium.ttf"))
			pDefaultFont = std::make_unique<Assets::Font>(*font);
		else
			NUCLEAR_WARN("[UI] Default font unavailable; shapes remain usable and labels reserve space.");
		mContext.SetDefaultFont(pDefaultFont.get());
		int width = 0;
		int height = 0;
		Core::Engine::Get().GetMainWindow()->GetSize(width, height);
		mContext.SetViewport(static_cast<float>(width), static_cast<float>(height));
		mInitialized = true;
		return true;
	}

	void UIModule::Shutdown()
	{
		mContext.Clear();
		mContext.SetDefaultFont(nullptr);
		pDefaultFont.reset();
		mRenderer.Shutdown();
		mInitialized = false;
	}

	void UIModule::Render()
	{
		if (!mInitialized)
			return;
		mRenderer.Render(mContext.BuildDrawList(), mContext.GetPixelSize(), mContext.GetScale());
	}
} // namespace Nuclear::UI
