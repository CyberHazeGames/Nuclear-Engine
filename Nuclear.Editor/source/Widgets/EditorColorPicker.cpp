#include <EditorColorPicker.h>
#include <EditorColorWheel.h>
#include <EditorWidgets.h>
#include <UI/UIContext.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Nuclear::Editor
{
	EditorColorPicker::EditorColorPicker(Math::Vector3 color,
		std::function<void(Math::Vector3)> committed, float maximum)
		: mMaximum(std::isfinite(maximum) ? std::max(1.f, maximum) : 1.f)
	{
		mCallback = [committed = std::move(committed)](Math::Vector4 value) {
			if (committed) committed(Math::Vector3(value));
		};
		Build({color, 1});
	}

	EditorColorPicker::EditorColorPicker(Math::Vector4 color,
		std::function<void(Math::Vector4)> committed, float maximum)
		: mCallback(std::move(committed)),
		mMaximum(std::isfinite(maximum) ? std::max(1.f, maximum) : 1.f), mEditAlpha(true)
	{
		Build(color);
	}

	void EditorColorPicker::Build(Math::Vector4 color)
	{
		auto root = std::make_unique<UI::VerticalBox>();
		root->SetSpacing(6);
		Label(*root, mEditAlpha ? "Wheel / Value / Alpha" : "Wheel / Value", 12);
		pWheel = &root->Add<EditorColorWheel>(mEditAlpha, mMaximum);
		pWheel->SetColor(color);
		pWheel->OnPreview = [this]() { Synchronize(); };
		pWheel->OnCommitted = [this]() { Commit(); };

		Label(*root, mEditAlpha ? "Linear RGBA" : "Linear RGB", 12);
		auto& rgb = root->Add<UI::HorizontalBox>();
		rgb.SetSpacing(4);
		Label(*root, "HSV (H degrees, S 0-1, V linear)", 12);
		auto& hsv = root->Add<UI::HorizontalBox>();
		hsv.SetSpacing(4);
		const char* names[] = {"Red", "Green", "Blue", "Alpha", "Hue", "Saturation", "Value"};
		for (int channel = 0; channel < 7; ++channel)
		{
			if (channel == 3 && !mEditAlpha) continue;
			auto& row = channel < 4 ? rgb : hsv;
			auto& field = row.Add<UI::VerticalBox>();
			field.SetSpacing(3);
			Fill(row, field);
			Label(field, names[channel], 11);
			auto& input = field.Add<UI::TextBox>();
			mInputs[channel] = &input;
			input.OnCommitted = [this, channel](const std::string& text) { CommitNumber(channel, text); };
		}
		Label(*root, mEditAlpha ? "HEX #RRGGBBAA (linear 8-bit)" : "HEX #RRGGBB (linear 8-bit)", 12);
		pHex = &root->Add<UI::TextBox>();
		pHex->OnCommitted = [this](const std::string& text) { CommitHex(text); };
		SetContent(std::move(root));
		mCommitted = pWheel->GetColor();
		Synchronize();
	}

	Math::Vector4 EditorColorPicker::GetColor() const { return pWheel->GetColor(); }
	void EditorColorPicker::SetColor(Math::Vector3 color) { SetColor(Math::Vector4(color, 1)); }
	void EditorColorPicker::SetColor(Math::Vector4 color)
	{
		if (auto* context = GetContext(); context && context->GetCapturedWidget() == pWheel->GetId())
			context->CancelSubtree(*pWheel);
		pWheel->SetColor(color);
		mCommitted = pWheel->GetColor();
		Synchronize();
	}

	void EditorColorPicker::Synchronize()
	{
		const auto color = pWheel->GetColor();
		const auto hsv = pWheel->GetHSV();
		for (int channel = 0; channel < 7; ++channel)
			if (mInputs[channel])
				mInputs[channel]->SetText(Decimal(channel < 4 ? color[channel] : hsv[channel - 4]));
		std::ostringstream hex;
		hex << '#' << std::uppercase << std::hex << std::setfill('0');
		for (int channel = 0; channel < (mEditAlpha ? 4 : 3); ++channel)
			hex << std::setw(2) << static_cast<int>(std::lround(std::clamp(color[channel], 0.f, 1.f) * 255));
		pHex->SetText(hex.str());
	}

	void EditorColorPicker::CommitNumber(int channel, const std::string& text)
	{
		float parsed;
		try
		{
			size_t count = 0;
			parsed = std::stof(text, &count);
			const float maximum = channel == 4 ? 360.f : channel == 3 || channel == 5 ? 1.f : mMaximum;
			if (count != text.size() || !std::isfinite(parsed) || parsed < 0 || parsed > maximum)
				throw std::invalid_argument("Color channel out of range");
		}
		catch (const std::exception&)
		{
			Synchronize();
			return;
		}
		if (channel < 4)
		{
			auto color = pWheel->GetColor();
			color[channel] = parsed;
			pWheel->SetColor(color);
		}
		else
		{
			auto hsv = pWheel->GetHSV();
			hsv[channel - 4] = parsed;
			pWheel->SetHSV(hsv);
		}
		Commit();
	}

	void EditorColorPicker::CommitHex(const std::string& text)
	{
		const size_t start = !text.empty() && text[0] == '#' ? 1 : 0;
		const int channels = mEditAlpha ? 4 : 3;
		if (text.size() != start + channels * 2)
		{
			Synchronize();
			return;
		}
		Math::Vector4 color(1);
		for (int channel = 0; channel < channels; ++channel)
		{
			int byte = 0;
			for (size_t digit = 0; digit < 2; ++digit)
			{
				const char character = text[start + channel * 2 + digit];
				const int value = character >= '0' && character <= '9' ? character - '0'
					: character >= 'A' && character <= 'F' ? character - 'A' + 10
					: character >= 'a' && character <= 'f' ? character - 'a' + 10 : -1;
				if (value < 0)
				{
					Synchronize();
					return;
				}
				byte = byte * 16 + value;
			}
			color[channel] = byte / 255.f;
		}
		pWheel->SetColor(color);
		Commit();
	}

	void EditorColorPicker::Commit()
	{
		Synchronize();
		const auto color = pWheel->GetColor();
		if (color == mCommitted) return;
		mCommitted = color;
		auto callback = mCallback;
		if (callback) callback(color);
	}
}
