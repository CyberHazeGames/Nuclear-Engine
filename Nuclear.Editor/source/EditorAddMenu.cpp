#include <EditorAddMenu.h>
#include <EditorWidgets.h>
#include <Nuclear.Platform/Input.h>
#include <UI/UIContext.h>

namespace Nuclear::Editor
{
	EditorAddMenu::EditorAddMenu(UI::Rect anchor, std::function<void(int)> create, std::function<void()> dismiss)
		: mAnchor(anchor), mCreate(std::move(create)), mDismiss(std::move(dismiss))
	{
		const char* labels[] = {"Empty Entity", "Shapes  >", "Skybox", "Box", "Plane", "Sphere"};
		for (size_t index = 0; index < mButtons.size(); ++index)
		{
			mButtons[index] = &Action(*this, labels[index], [this, index]() { Execute(index); });
			if (index >= 3)
				mButtons[index]->SetVisibility(UI::Visibility::Collapsed);
		}
		Highlight();
	}

	void EditorAddMenu::SetShapesOpen(bool open)
	{
		mShapesOpen = open;
		for (size_t index = 3; index < mButtons.size(); ++index)
			mButtons[index]->SetVisibility(open ? UI::Visibility::Visible : UI::Visibility::Collapsed);
		InvalidateLayout();
	}

	void EditorAddMenu::Highlight()
	{
		for (size_t index = 0; index < mButtons.size(); ++index)
		{
			UI::ControlStyle style;
			style.Normal = index == mSelected ? UI::Color{.08f, .22f, .24f, 1} : UI::Color{.065f, .08f, .10f, 1};
			style.Hovered = {.08f, .22f, .24f, 1};
			style.Pressed = {.035f, .15f, .17f, 1};
			mButtons[index]->SetStyle(style);
		}
	}

	void EditorAddMenu::Execute(size_t index)
	{
		if (index == 1)
		{
			SetShapesOpen(true);
			mSelected = 3;
			Highlight();
			GetContext()->SetFocus(GetId());
			return;
		}
		const int shapes[] = {0, 0, 4, 1, 3, 2};
		auto create = mCreate;
		const int shape = shapes[index];
		mDismiss();
		create(shape);
	}

	void EditorAddMenu::OnArrange(UI::Rect content)
	{
		const float width = std::min(220.f, content.Width);
		const float height = std::min(116.f, content.Height);
		mMenu = {std::clamp(mAnchor.x, content.x, content.x + content.Width - width),
			std::clamp(mAnchor.y + mAnchor.Height, content.y, content.y + content.Height - height), width, height};
		const float right = mMenu.x + width;
		const float left = right + width <= content.x + content.Width ? right : mMenu.x - width;
		mShapes = {std::clamp(left, content.x, content.x + content.Width - width),
			std::clamp(mMenu.y + 36, content.y, content.y + content.Height - height), width, height};
		for (size_t index = 0; index < mButtons.size(); ++index)
		{
			const auto& box = index < 3 ? mMenu : mShapes;
			mButtons[index]->Arrange({box.x + 4, box.y + 4 + float(index % 3) * 36, width - 8, 36});
		}
	}

	void EditorAddMenu::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		for (int index = 0; index < (mShapesOpen ? 2 : 1); ++index)
		{
			const auto& box = index == 0 ? mMenu : mShapes;
			list.Add(box, clip, {.18f, .23f, .27f, 1});
			list.Add({box.x + 1, box.y + 1, box.Width - 2, box.Height - 2}, clip, {.065f, .08f, .10f, 1});
		}
	}

	UI::EventReply EditorAddMenu::OnInput(const UI::InputEvent& event)
	{
		using Type = UI::InputEvent::Type;
		using Key = Platform::Input;
		if (event.EventType == Type::Cancel)
		{
			auto* context = GetContext();
			const auto id = GetId();
			context->Defer([context, id]() {
				auto* menu = dynamic_cast<EditorAddMenu*>(context->Find(id));
				if (!menu)
					return;
				for (auto* widget = context->Find(context->GetFocusedWidget()); widget; widget = widget->GetParent())
					if (widget == menu)
						return;
				menu->mDismiss();
			});
			return {true};
		}
		if (event.EventType == Type::KeyDown && !event.Repeat)
		{
			if (event.Key == Key::KEYCODE_ESCAPE)
				mDismiss();
			else if (event.Key == Key::KEYCODE_LEFT && mShapesOpen)
			{
				SetShapesOpen(false);
				mSelected = 1;
			}
			else if (event.Key == Key::KEYCODE_RIGHT && mSelected == 1)
				Execute(1);
			else if (event.Key == Key::KEYCODE_RETURN)
				Execute(mSelected);
			else if (event.Key == Key::KEYCODE_DOWN || event.Key == Key::KEYCODE_UP)
			{
				const size_t first = mSelected >= 3 ? 3 : 0;
				const size_t step = event.Key == Key::KEYCODE_DOWN ? 1 : 2;
				mSelected = first + (mSelected - first + step) % 3;
				if (first == 0 && mSelected != 1)
					SetShapesOpen(false);
			}
			else
				return {};
			Highlight();
			GetContext()->SetFocus(GetId());
			return {true};
		}
		if (event.EventType == Type::PointerMove && mMenu.Contains(event.Position))
		{
			for (size_t index = 0; index < 3; ++index)
				if (mButtons[index]->GetBounds().Contains(event.Position))
				{
					mSelected = index;
					SetShapesOpen(index == 1);
					Highlight();
				}
		}
		if (event.EventType == Type::PointerDown && !mMenu.Contains(event.Position) &&
			(!mShapesOpen || !mShapes.Contains(event.Position)))
		{
			mDismiss();
			return {true};
		}
		return {};
	}
}
