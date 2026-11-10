#include <TexturePreviewViewport.h>
#include <Assets/AssetLibrary.h>
#include <UI/DrawList.h>
#include <algorithm>

namespace Nuclear::Editor
{
	TexturePreviewViewport::TexturePreviewViewport(const Core::UUID& texture) : mTexture(texture) {}

	void TexturePreviewViewport::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		const auto bounds = GetBounds();
		list.Add(bounds, clip, {.055f, .065f, .08f, 1});
		auto* texture = Assets::AssetLibrary::Get().mImportedTextures.GetAsset(mTexture);
		if (!texture || texture->GetState() != Assets::IAsset::State::Created || !texture->GetTextureView() ||
			!texture->GetWidth() || !texture->GetHeight()) return;
		const float fit = std::min(bounds.Width / texture->GetWidth(), bounds.Height / texture->GetHeight());
		const float width = texture->GetWidth() * fit;
		const float height = texture->GetHeight() * fit;
		const UI::Rect image{bounds.x + (bounds.Width - width) * .5f, bounds.y + (bounds.Height - height) * .5f, width, height};
		for (float y = 0; y < height; y += 24)
			for (float x = 0; x < width; x += 24)
			{
				const bool alternate = (int(x / 24) + int(y / 24)) % 2;
				list.Add({image.x + x, image.y + y, std::min(24.f, width - x), std::min(24.f, height - y)}, clip,
					alternate ? UI::Color{.18f, .2f, .22f, 1} : UI::Color{.11f, .13f, .15f, 1});
			}
		list.Add(image, clip, {1, 1, 1, 1}, texture->GetTextureView());
	}
}
