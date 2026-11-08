#include <UI/UIContext.h>
#include <UI/ScrollBox.h>
#include <cmath>
#include <stdexcept>

namespace Nuclear::UI
{
	UIContext::~UIContext()
	{
		Clear();
	}

	Widget& UIContext::AddToViewport(std::unique_ptr<Widget> root, int zOrder)
	{
		if (!root || root->GetContext() || root->GetParent())
			throw std::invalid_argument("Viewport root must be detached");
		auto& result = *root;
		mRoots.push_back({std::move(root), zOrder});
		std::stable_sort(mRoots.begin(), mRoots.end(),
						 [](const auto& left, const auto& right) { return left.ZOrder < right.ZOrder; });
		result.Attach(this, nullptr);
		InvalidateLayout();
		return result;
	}

	void UIContext::Defer(std::function<void()> mutation)
	{
		mDeferred.push_back(std::move(mutation));
		if (!mDispatchDepth)
			FlushDeferred();
	}

	void UIContext::FlushDeferred()
	{
		if (mDispatchDepth)
			return;
		++mDispatchDepth;
		try
		{
			while (!mDeferred.empty())
			{
				auto mutations = std::move(mDeferred);
				mDeferred.clear();
				for (auto& mutation : mutations)
					mutation();
			}
		}
		catch (...)
		{
			--mDispatchDepth;
			throw;
		}
		--mDispatchDepth;
	}

	void UIContext::RemoveFromViewport(WidgetId id)
	{
		Defer(
			[this, id]()
			{
				for (auto iterator = mRoots.begin(); iterator != mRoots.end(); ++iterator)
				{
					if (iterator->Root->GetId() != id)
						continue;
					CancelSubtree(*iterator->Root);
					auto root = std::move(iterator->Root);
					mRoots.erase(iterator);
					root->Attach(nullptr, nullptr);
					mDrawList.Quads.clear();
					InvalidateLayout();
					break;
				}
			});
	}

	void UIContext::Clear()
	{
		Defer(
			[this]()
			{
				CancelInput();
				auto roots = std::move(mRoots);
				mRoots.clear();
				for (auto& entry : roots)
					entry.Root->Attach(nullptr, nullptr);
				mDrawList.Quads.clear();
				mInputMode = InputMode::GameAndUI;
				OnPreviewEvent = {};
				mShiftDown = false;
				InvalidateLayout();
			});
	}

	Widget* UIContext::Find(WidgetId id) const
	{
		if (!id)
			return nullptr;
		auto find = [id](auto&& self, Widget& widget) -> Widget*
		{
			if (widget.GetId() == id)
				return &widget;
			for (auto* child : widget.Children())
				if (auto* result = self(self, *child))
					return result;
			return nullptr;
		};
		for (const auto& entry : mRoots)
			if (auto* result = find(find, *entry.Root))
				return result;
		return nullptr;
	}

	void UIContext::SetViewport(float width, float height, float scale)
	{
		width = std::isfinite(width) ? std::max(0.0f, width) : 0;
		height = std::isfinite(height) ? std::max(0.0f, height) : 0;
		scale = std::isfinite(scale) && scale > 0 ? scale : 1;
		if (width == mPixelSize.x && height == mPixelSize.y && scale == mScale)
			return;
		mPixelSize = {width, height};
		mScale = scale;
		if (!width || !height)
			CancelInput();
		InvalidateLayout();
	}

	void UIContext::SetDefaultFont(const Assets::Font* font)
	{
		pDefaultFont = font;
		InvalidateLayout();
	}

	void UIContext::Layout()
	{
		if (!mLayoutDirty)
			return;
		mLayoutDirty = false;
		const Vec2 logical = {mPixelSize.x / mScale, mPixelSize.y / mScale};
		for (auto& entry : mRoots)
		{
			entry.Root->Measure(logical);
			entry.Root->Arrange({0, 0, logical.x, logical.y});
		}
	}

	const DrawList& UIContext::BuildDrawList()
	{
		Layout();
		mDrawList.Quads.clear();
		const Rect clip = {0, 0, mPixelSize.x / mScale, mPixelSize.y / mScale};
		for (auto& entry : mRoots)
			entry.Root->Paint(mDrawList, clip);
		return mDrawList;
	}

	void UIContext::SetFocus(WidgetId id)
	{
		if (id == mFocus)
			return;
		const auto previous = mFocus;
		mFocus = 0;
		if (auto* widget = Find(previous))
			widget->OnInput({InputEvent::Type::Cancel});
		if (auto* widget = Find(id); widget && widget->IsFocusable() && widget->IsEnabled() && widget->IsVisible())
		{
			mFocus = id;
			Layout();
			for (auto* parent = widget->GetParent(); parent; parent = parent->GetParent())
			{
				if (auto* scroll = dynamic_cast<ScrollBox*>(parent))
				{
					scroll->ScrollIntoView(*widget);
					Layout();
				}
			}
		}
	}

	void UIContext::CancelInput()
	{
		const auto capture = mCapture;
		mCapture = 0;
		if (auto* widget = Find(capture))
			widget->OnInput({InputEvent::Type::Cancel});
		SetFocus(0);
		if (auto* widget = Find(mHover))
			widget->mHovered = false;
		mHover = 0;
		mShiftDown = false;
	}

	void UIContext::CancelSubtree(Widget& subtree)
	{
		auto inSubtree = [&subtree](Widget* widget)
		{
			for (; widget; widget = widget->GetParent())
				if (widget == &subtree)
					return true;
			return false;
		};
		if (inSubtree(Find(mCapture)))
		{
			const auto captured = mCapture;
			mCapture = 0;
			if (auto* widget = Find(captured))
				widget->OnInput({InputEvent::Type::Cancel});
		}
		if (inSubtree(Find(mFocus)))
			SetFocus(0);
		if (auto* widget = Find(mHover); inSubtree(widget))
		{
			widget->mHovered = false;
			mHover = 0;
		}
	}

	void UIContext::SetInputMode(InputMode mode)
	{
		if (mode == InputMode::GameOnly)
			CancelInput();
		mInputMode = mode;
	}

	void UIContext::ResetInput()
	{
		++mDispatchDepth;
		CancelInput();
		--mDispatchDepth;
		FlushDeferred();
	}

	bool UIContext::WantsMouse() const
	{
		return mWindowFocused && mInputMode != InputMode::GameOnly && mPixelSize.x > 0 && mPixelSize.y > 0 &&
			   (mInputMode == InputMode::UIOnly || mCapture || mHover);
	}
	bool UIContext::WantsKeyboard() const
	{
		return mWindowFocused && mInputMode != InputMode::GameOnly && mPixelSize.x > 0 && mPixelSize.y > 0 &&
			   (mInputMode == InputMode::UIOnly || mFocus);
	}

	void UIContext::UpdateHover(Vec2 position)
	{
		if (auto* previous = Find(mHover))
			previous->mHovered = false;
		mHover = 0;
		Rect clip = {0, 0, mPixelSize.x / mScale, mPixelSize.y / mScale};
		for (auto iterator = mRoots.rbegin(); iterator != mRoots.rend(); ++iterator)
		{
			if (auto* target = iterator->Root->HitTest(position, clip))
			{
				mHover = target->GetId();
				target->mHovered = true;
				break;
			}
		}
	}

	bool UIContext::Dispatch(WidgetId target, const InputEvent& event)
	{
		std::vector<WidgetId> route;
		for (auto* widget = Find(target); widget; widget = widget->GetParent())
			route.push_back(widget->GetId());
		for (auto id : route)
		{
			auto* widget = Find(id);
			if (!widget || !widget->IsEnabled() || !widget->IsVisible())
				continue;
			const auto reply = widget->OnInput(event);
			if (reply.Focus)
				SetFocus(id);
			if (reply.Capture && widget->IsEnabled() && widget->IsVisible())
				mCapture = id;
			if (reply.Release && mCapture == id)
				mCapture = 0;
			if (reply.Handled)
				return true;
		}
		return false;
	}

	void UIContext::CollectFocusable(Widget& widget, std::vector<WidgetId>& ids) const
	{
		if (!widget.IsEnabled() || widget.GetVisibility() != Visibility::Visible)
			return;
		if (widget.IsFocusable())
			ids.push_back(widget.GetId());
		for (auto* child : widget.Children())
			CollectFocusable(*child, ids);
	}

	bool UIContext::ProcessEvent(const Platform::Event& event)
	{
		// Keep all callbacks, including Cancel/focus callbacks, inside a destruction guard.
		++mDispatchDepth;
		bool handled = false;
		try
		{
			const bool acceptsInput = mWindowFocused && mInputMode != InputMode::GameOnly &&
				mPixelSize.x > 0 && mPixelSize.y > 0;
			const bool previewHandled = (acceptsInput || event.mType == Platform::EventType::FocusChanged) &&
				OnPreviewEvent && OnPreviewEvent(event);
			if (event.mType == Platform::EventType::FocusChanged)
			{
				mWindowFocused = event.mFocused;
				if (!mWindowFocused)
					CancelInput();
			}
			else if (event.mType == Platform::EventType::MouseLeave)
			{
				if (auto* widget = Find(mHover))
					widget->mHovered = false;
				mHover = 0;
			}
			else if (previewHandled)
				handled = true;
			else if (acceptsInput)
			{
				Layout();
				InputEvent input;
				if (event.mType == Platform::EventType::MouseMove || event.mType == Platform::EventType::MouseButton)
				{
					mPointer = {event.mMouseX / mScale, event.mMouseY / mScale};
					UpdateHover(mPointer);
					input.Position = mPointer;
					input.Button = event.mButton;
					input.EventType =
						event.mType == Platform::EventType::MouseMove
							? InputEvent::Type::PointerMove
							: (event.mPressed ? InputEvent::Type::PointerDown : InputEvent::Type::PointerUp);
					const bool captured = mCapture != 0;
					if (input.EventType == InputEvent::Type::PointerDown && !mHover && !mCapture)
						SetFocus(0);
					handled = Dispatch(mCapture ? mCapture : mHover, input) || captured || mHover ||
							  mInputMode == InputMode::UIOnly;
				}
				else if (event.mType == Platform::EventType::Key)
				{
					if (event.mKey == Platform::Input::KEYCODE_LSHIFT || event.mKey == Platform::Input::KEYCODE_RSHIFT)
						mShiftDown = event.mPressed;
					if (event.mKey == Platform::Input::KEYCODE_TAB && event.mPressed)
					{
						std::vector<WidgetId> focusable;
						for (auto& entry : mRoots)
							CollectFocusable(*entry.Root, focusable);
						if (!focusable.empty() && !event.mRepeat)
						{
							auto current = std::find(focusable.begin(), focusable.end(), mFocus);
							int index = current == focusable.end() ? (mShiftDown ? 0 : -1)
																   : static_cast<int>(current - focusable.begin());
							const int count = static_cast<int>(focusable.size());
							index = (index + (mShiftDown ? count - 1 : 1)) % count;
							SetFocus(focusable[index]);
						}
						handled = !focusable.empty() || mInputMode == InputMode::UIOnly;
					}
					else
					{
						input.EventType = event.mPressed ? InputEvent::Type::KeyDown : InputEvent::Type::KeyUp;
						input.Key = event.mKey;
						input.Repeat = event.mRepeat;
						handled = Dispatch(mFocus, input) || WantsKeyboard();
					}
				}
				else if (event.mType == Platform::EventType::MouseWheel)
				{
					UpdateHover(mPointer);
					input.EventType = InputEvent::Type::Wheel;
					input.Position = mPointer;
					input.WheelY = event.mWheelY;
					handled = Dispatch(mCapture ? mCapture : mHover, input) || WantsMouse();
				}
				else if (event.mType == Platform::EventType::TextInput)
				{
					input.EventType = InputEvent::Type::Text;
					input.Codepoint = event.mCodepoint;
					handled = Dispatch(mFocus, input) || WantsKeyboard();
				}
			}
		}
		catch (...)
		{
			--mDispatchDepth;
			FlushDeferred();
			throw;
		}
		--mDispatchDepth;
		FlushDeferred();
		return handled;
	}
} // namespace Nuclear::UI
