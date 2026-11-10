#include <EditorColorWheel.h>
#include <Nuclear.Platform/Input.h>
#include <UI/DrawList.h>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Nuclear::Editor
{
	namespace
	{
		constexpr float Tau = 2 * std::numbers::pi_v<float>;

		float Bounded(float value, float maximum)
		{
			return std::isfinite(value) ? std::clamp(value, 0.f, maximum) : 0.f;
		}

		UI::Color RGB(Math::Vector3 hsv, float alpha = 1)
		{
			const float sector = hsv.x / 60.f;
			const float chroma = hsv.z * hsv.y;
			const float second = chroma * (1 - std::abs(std::fmod(sector, 2.f) - 1));
			const float base = hsv.z - chroma;
			UI::Color color;
			if (sector < 1) color = {chroma, second, 0, alpha};
			else if (sector < 2) color = {second, chroma, 0, alpha};
			else if (sector < 3) color = {0, chroma, second, alpha};
			else if (sector < 4) color = {0, second, chroma, alpha};
			else if (sector < 5) color = {second, 0, chroma, alpha};
			else color = {chroma, 0, second, alpha};
			return {color.r + base, color.g + base, color.b + base, alpha};
		}

		void Outline(UI::DrawList& list, UI::Rect rect, UI::Rect clip, UI::Color color)
		{
			list.Add({rect.x, rect.y, rect.Width, 1}, clip, color);
			list.Add({rect.x, rect.y + rect.Height - 1, rect.Width, 1}, clip, color);
			list.Add({rect.x, rect.y, 1, rect.Height}, clip, color);
			list.Add({rect.x + rect.Width - 1, rect.y, 1, rect.Height}, clip, color);
		}

		void Checker(UI::DrawList& list, UI::Rect rect, UI::Rect clip)
		{
			for (float y = 0; y < rect.Height; y += 8)
				for (float x = 0; x < rect.Width; x += 8)
				{
					const float shade = (int(x / 8) + int(y / 8)) % 2 ? .32f : .12f;
					list.Add({rect.x + x, rect.y + y, std::min(8.f, rect.Width - x), std::min(8.f, rect.Height - y)},
						clip, {shade, shade, shade, 1});
				}
		}
	}

	EditorColorWheel::EditorColorWheel(bool alpha, float maximum)
		: mMaximum(std::isfinite(maximum) ? std::max(1.f, maximum) : 1.f), mEditAlpha(alpha)
	{
	}

	void EditorColorWheel::SetColor(Math::Vector4 color)
	{
		mDragging = -1;
		const float red = Bounded(color.r, mMaximum);
		const float green = Bounded(color.g, mMaximum);
		const float blue = Bounded(color.b, mMaximum);
		mRGB = {red, green, blue};
		const float minimum = std::min({red, green, blue});
		const float value = std::max({red, green, blue});
		const float chroma = value - minimum;
		if (chroma > 0)
		{
			float hue = value == red ? (green - blue) / chroma
				: value == green ? (blue - red) / chroma + 2 : (red - green) / chroma + 4;
			mHSV.x = std::fmod(hue * 60 + 360, 360.f);
		}
		// Preserve saturation at black so increasing value restores the chosen hue.
		if (value > 0)
			mHSV.y = chroma / value;
		mHSV.z = value;
		mAlpha = mEditAlpha ? Bounded(color.a, 1) : 1;
	}

	void EditorColorWheel::SetHSV(Math::Vector3 hsv)
	{
		mHSV = {std::isfinite(hsv.x) ? std::fmod(std::fmod(hsv.x, 360.f) + 360, 360.f) : 0,
			Bounded(hsv.y, 1), Bounded(hsv.z, mMaximum)};
		const auto color = RGB(mHSV);
		mRGB = {color.r, color.g, color.b};
	}

	Math::Vector4 EditorColorWheel::GetColor() const
	{
		return {mRGB, mAlpha};
	}

	Math::Vector3 EditorColorWheel::GetHSV() const { return mHSV; }
	bool EditorColorWheel::IsFocusable() const { return true; }
	UI::Vec2 EditorColorWheel::OnMeasure(UI::Vec2) { return {220, 202}; }

	UI::Rect EditorColorWheel::Region(int index) const
	{
		const auto bounds = ContentBounds();
		const float reserved = mEditAlpha ? 56.f : 28.f;
		const float diameter = std::max(0.f, std::min({176.f, bounds.Width - reserved, bounds.Height - 26}));
		const float left = bounds.x + std::max(0.f, (bounds.Width - diameter - reserved) * .5f);
		if (index == 0) return {left, bounds.y, diameter, diameter};
		if (index == 1) return {left + diameter + 8, bounds.y, 16, diameter};
		if (index == 2) return {left + diameter + 36, bounds.y, 16, diameter};
		return {left, bounds.y + diameter + 8, diameter + reserved - 4, 16};
	}

	void EditorColorWheel::Drag(UI::Vec2 position)
	{
		const auto rect = Region(mDragging);
		if (mDragging == 0 && rect.Width > 0)
		{
			const float x = (position.x - rect.x - rect.Width * .5f) / (rect.Width * .5f);
			const float y = (rect.y + rect.Height * .5f - position.y) / (rect.Height * .5f);
			const float radius = std::hypot(x, y);
			if (radius > .001f)
				mHSV.x = std::fmod(std::atan2(y, x) / Tau * 360 + 360, 360.f);
			mHSV.y = std::min(1.f, radius);
		}
		else if (rect.Height > 0)
		{
			const float value = std::clamp(1 - (position.y - rect.y) / rect.Height, 0.f, 1.f);
			if (mDragging == 1) mHSV.z = value * mDragValueMaximum;
			if (mDragging == 2) mAlpha = value;
		}
		if (mDragging != 2) SetHSV(mHSV);
		if (OnPreview) OnPreview();
	}

	UI::EventReply EditorColorWheel::OnInput(const UI::InputEvent& event)
	{
		using Type = UI::InputEvent::Type;
		if (event.EventType == Type::Cancel ||
			(event.EventType == Type::KeyDown && event.Key == Platform::Input::KEYCODE_ESCAPE && mDragging >= 0))
		{
			if (mDragging >= 0)
			{
				mHSV = mStartHSV;
				mRGB = mStartRGB;
				mAlpha = mStartAlpha;
				mDragging = -1;
				if (OnPreview) OnPreview();
			}
			return {true, false, true};
		}
		if (event.EventType == Type::PointerDown && event.Button == MOUSE_BUTTON_LEFT)
		{
			for (int index = 0; index < (mEditAlpha ? 3 : 2); ++index)
			{
				const auto rect = Region(index);
				if (!rect.Contains(event.Position)) continue;
				if (index == 0 && std::hypot(event.Position.x - rect.x - rect.Width * .5f,
					event.Position.y - rect.y - rect.Height * .5f) > rect.Width * .5f) continue;
				mStartHSV = mHSV;
				mStartRGB = mRGB;
				mStartAlpha = mAlpha;
				mDragValueMaximum = std::max(1.f, mHSV.z);
				mDragging = index;
				Drag(event.Position);
				return {true, true, false, true};
			}
			return {true, false, false, true};
		}
		if (mDragging >= 0 && event.EventType == Type::PointerMove)
		{
			Drag(event.Position);
			return {true};
		}
		if (mDragging >= 0 && event.EventType == Type::PointerUp && event.Button == MOUSE_BUTTON_LEFT)
		{
			Drag(event.Position);
			mDragging = -1;
			auto callback = OnCommitted;
			if (callback) callback();
			return {true, false, true};
		}
		if (event.EventType == Type::KeyDown && mDragging < 0)
		{
			auto hsv = mHSV;
			if (event.Key == Platform::Input::KEYCODE_LEFT) hsv.x -= 1;
			else if (event.Key == Platform::Input::KEYCODE_RIGHT) hsv.x += 1;
			else if (event.Key == Platform::Input::KEYCODE_UP) hsv.y += .01f;
			else if (event.Key == Platform::Input::KEYCODE_DOWN) hsv.y -= .01f;
			else return {};
			SetHSV(hsv);
			auto callback = OnCommitted;
			if (callback) callback();
			return {true};
		}
		return {};
	}

	void EditorColorWheel::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		const auto disk = Region(0);
		const float radius = disk.Width * .5f;
		// Small solid quads batch together in the retained renderer; no GPU resources are owned here.
		constexpr float step = 1;
		for (float y = 0; y < disk.Height; y += step)
			for (float x = 0; x < disk.Width; x += step)
			{
				const float dx = x + step * .5f - radius;
				const float dy = radius - y - step * .5f;
				const float distance = std::hypot(dx, dy);
				if (distance > radius + .5f || radius <= 0) continue;
				const float hue = std::fmod(std::atan2(dy, dx) / Tau * 360 + 360, 360.f);
				const float coverage = std::clamp(radius + .5f - distance, 0.f, 1.f);
				list.Add({disk.x + x, disk.y + y, step, step}, clip, RGB({hue, std::min(1.f, distance / radius), 1}, coverage));
			}
		const float angle = mHSV.x / 360 * Tau;
		const float markerX = disk.x + radius + std::cos(angle) * mHSV.y * radius;
		const float markerY = disk.y + radius - std::sin(angle) * mHSV.y * radius;
		Outline(list, {markerX - 4, markerY - 4, 9, 9}, clip, {0, 0, 0, 1});
		Outline(list, {markerX - 3, markerY - 3, 7, 7}, clip, {1, 1, 1, 1});
		const float valueMaximum = mDragging == 1 ? mDragValueMaximum : std::max(1.f, mHSV.z);
		for (int index = 1; index < (mEditAlpha ? 3 : 2); ++index)
		{
			const auto strip = Region(index);
			if (index == 2) Checker(list, strip, clip);
			for (float y = 0; y < strip.Height; y += step)
			{
				const float value = 1 - y / strip.Height;
				const auto color = index == 1 ? RGB({mHSV.x, mHSV.y, value * valueMaximum}) : RGB(mHSV, value);
				list.Add({strip.x, strip.y + y, strip.Width, std::min(step, strip.Height - y)}, clip, color);
			}
			const float selected = index == 1 ? mHSV.z / valueMaximum : mAlpha;
			Outline(list, {strip.x - 2, strip.y + (1 - selected) * strip.Height - 2, strip.Width + 4, 5}, clip, {1, 1, 1, 1});
		}
		const auto swatch = Region(3);
		Checker(list, swatch, clip);
		list.Add(swatch, clip, RGB(mHSV, mAlpha));
		if (HasFocus()) Outline(list, GetBounds(), clip, {.3f, .7f, 1, 1});
	}
}
