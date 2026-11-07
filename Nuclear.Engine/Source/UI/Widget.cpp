#include <UI/DrawList.h>
#include <UI/UIContext.h>
#include <UI/Widget.h>
#include <atomic>
#include <cmath>

namespace Nuclear::UI
{
	namespace
	{
		std::atomic<WidgetId> NextWidgetId{1};
		float Nonnegative(float value)
		{
			return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
		}
	} // namespace

	Widget::Widget() : mId(NextWidgetId.fetch_add(1)) {}

	Widget::~Widget() = default;

	void Widget::Attach(UIContext* context, Widget* parent)
	{
		const bool changed = pContext != context;
		if (pContext && changed)
			OnRemoved();
		pContext = context;
		pParent = parent;
		if (context && changed)
			OnAttached();
		for (auto* child : Children())
			child->Attach(context, this);
	}

	void Widget::SetVisibility(Visibility visibility)
	{
		if (mVisibility == visibility)
			return;
		if (visibility != Visibility::Visible && pContext)
			pContext->CancelSubtree(*this);
		mVisibility = visibility;
		InvalidateLayout();
	}

	void Widget::SetEnabled(bool enabled)
	{
		if (!enabled && pContext)
			pContext->CancelSubtree(*this);
		mEnabled = enabled;
	}

	bool Widget::IsEnabled() const
	{
		return mEnabled && (!pParent || pParent->IsEnabled());
	}

	bool Widget::IsVisible() const
	{
		return mVisibility == Visibility::Visible && (!pParent || pParent->IsVisible());
	}

	bool Widget::HasFocus() const
	{
		return pContext && pContext->GetFocusedWidget() == mId;
	}

	void Widget::SetPadding(Margin padding)
	{
		mPadding = {Nonnegative(padding.Left), Nonnegative(padding.Top), Nonnegative(padding.Right),
					Nonnegative(padding.Bottom)};
		InvalidateLayout();
	}

	void Widget::SetSizeLimits(Vec2 minimum, Vec2 maximum)
	{
		mMinimum = {Nonnegative(minimum.x), Nonnegative(minimum.y)};
		mMaximum = {std::max(mMinimum.x, Nonnegative(maximum.x)), std::max(mMinimum.y, Nonnegative(maximum.y))};
		InvalidateLayout();
	}

	void Widget::InvalidateLayout()
	{
		if (pContext)
			pContext->InvalidateLayout();
	}

	Rect Widget::ContentBounds() const
	{
		return {mBounds.x + mPadding.Left, mBounds.y + mPadding.Top,
				std::max(0.0f, mBounds.Width - mPadding.Left - mPadding.Right),
				std::max(0.0f, mBounds.Height - mPadding.Top - mPadding.Bottom)};
	}

	void Widget::Measure(Vec2 available)
	{
		if (mVisibility == Visibility::Collapsed)
		{
			mDesiredSize = {};
			return;
		}
		const float horizontal = mPadding.Left + mPadding.Right;
		const float vertical = mPadding.Top + mPadding.Bottom;
		auto size = OnMeasure({std::max(0.0f, available.x - horizontal), std::max(0.0f, available.y - vertical)});
		mDesiredSize = {std::clamp(Nonnegative(size.x) + horizontal, mMinimum.x, mMaximum.x),
						std::clamp(Nonnegative(size.y) + vertical, mMinimum.y, mMaximum.y)};
	}

	void Widget::Arrange(Rect bounds)
	{
		bounds.Width = Nonnegative(bounds.Width);
		bounds.Height = Nonnegative(bounds.Height);
		mBounds = bounds;
		OnArrange(ContentBounds());
	}

	void Widget::Paint(DrawList& list, Rect clip) const
	{
		if (mVisibility != Visibility::Visible)
			return;
		if (mClipToBounds)
			clip = clip.Intersect(mBounds);
		if (clip.Width <= 0 || clip.Height <= 0)
			return;
		OnPaint(list, clip);
		for (auto* child : Children())
			child->Paint(list, clip);
	}

	Widget* Widget::HitTest(Vec2 position, Rect clip)
	{
		if (mVisibility != Visibility::Visible || !IsEnabled())
			return nullptr;
		if (mClipToBounds)
			clip = clip.Intersect(mBounds);
		if (!clip.Contains(position))
			return nullptr;
		auto children = Children();
		for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
		{
			if (auto* target = (*iterator)->HitTest(position, clip))
				return target;
		}
		return AcceptsPointer() && mBounds.Contains(position) ? this : nullptr;
	}

	Vec2 Widget::OnMeasure(Vec2)
	{
		return {};
	}

	void Widget::OnArrange(Rect) {}

	void Widget::OnPaint(DrawList&, Rect) const {}

	EventReply Widget::OnInput(const InputEvent&)
	{
		return {};
	}

	std::vector<Widget*> Widget::Children() const
	{
		return {};
	}

	const Assets::Font* Widget::ResolveFont(const Assets::Font* font) const
	{
		return font ? font : (pContext ? pContext->GetDefaultFont() : nullptr);
	}
} // namespace Nuclear::UI
