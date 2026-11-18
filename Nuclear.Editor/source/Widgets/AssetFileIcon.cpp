#include <AssetFileIcon.h>
#include <UI/DrawList.h>

namespace Nuclear::Editor
{
	AssetFileIcon::AssetFileIcon(bool folder, std::string format, UI::Color accent)
		: mFolder(folder), mFormat(std::move(format)), mAccent(accent)
	{
		SetSizeLimits({88, 48}, {88, 48});
	}

	void AssetFileIcon::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		const auto box = GetBounds();
		list.Add(box, clip, {.048f, .056f, .069f, 1});
		float x = box.x + box.Width * .5f, y = box.y + box.Height * .5f;
		if (mFolder)
		{
			list.Add({x - 22, y - 15, 17, 6}, clip, {.68f, .41f, .16f, 1});
			list.Add({x - 22, y - 11, 43, 25}, clip, {.73f, .48f, .20f, 1});
			list.Add({x - 21, y - 6, 43, 21}, clip, mAccent);
			list.Add({x - 21, y - 6, 43, 2}, clip, {1, .77f, .43f, 1});
			return;
		}
		if (mFormat == "MAP")
		{
			// Three folded panels with a continuous route across their faces.
			for (int i = 0; i < 36; ++i)
			{
				const int panel = i / 12, offset = i % 12;
				const float fold = (panel == 1 ? 11 - offset : offset) * .34f;
				const auto face = panel == 1 ? UI::Color{.27f, .51f, .38f, 1} : mAccent;
				list.Add({x - 18 + i, y - 14 + fold, 1, 24}, clip, face);
				const float route = (i < 16 ? i : 32 - i) * .39f;
				list.Add({x - 18 + i, y - 4 + fold + route, 1, 2}, clip, {.78f, .90f, .72f, 1});
			}
			return;
		}
		if (mFormat == "PFB")
		{
			list.Add({x - 4, y - 5, 8, 11}, clip, {.34f, .43f, .57f, 1});
			list.Add({x - 14, y + 3, 28, 3}, clip, {.34f, .43f, .57f, 1});
			list.Add({x - 7, y - 16, 14, 12}, clip, mAccent);
			list.Add({x - 19, y + 5, 12, 11}, clip, mAccent);
			list.Add({x + 7, y + 5, 12, 11}, clip, mAccent);
			return;
		}
		list.Add({x - 13, y - 17, 21, 33}, clip, {.34f, .40f, .49f, 1});
		list.Add({x + 8, y - 12, 5, 28}, clip, {.34f, .40f, .49f, 1});
		list.Add({x - 8, y - 11, 12, 2}, clip, {.67f, .73f, .80f, 1});
		list.Add({x - 16, y, 33, 12}, clip, mAccent);
		list.Text(ResolveFont(nullptr), mFormat, {x - 16, y, 33, 12}, clip, 10,
			{.05f, .06f, .075f, 1}, UI::Alignment::Center, false);
	}
}
