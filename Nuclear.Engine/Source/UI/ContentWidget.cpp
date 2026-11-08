#include <UI/ContentWidget.h>
#include <UI/UIContext.h>
#include <stdexcept>

namespace Nuclear::UI
{
	Widget& ContentWidget::AddChild(std::unique_ptr<Widget> child, const Slot& slot)
	{
		if (!mChildren.empty())
			throw std::logic_error("ContentWidget accepts only one child; use SetContent");
		return PanelWidget::AddChild(std::move(child), slot);
	}

	void ContentWidget::SetContent(std::unique_ptr<Widget> child)
	{
		if (child && (child->GetParent() || child->GetContext()))
			throw std::invalid_argument("Content must be detached");
		if (auto* context = GetContext())
		{
			auto holder = std::make_shared<std::unique_ptr<Widget>>(std::move(child));
			auto id = GetId();
			context->Defer(
				[context, id, holder]()
				{
					auto* content = dynamic_cast<ContentWidget*>(context->Find(id));
					if (!content)
						return;
					if (!content->mChildren.empty())
					{
						// Cancellation resolves focus/capture through the attached tree. Keep
						// its child entry intact until those lookups and callbacks have finished.
						context->CancelSubtree(*content->mChildren.front().Child);
						auto old = std::move(content->mChildren.front().Child);
						content->mChildren.clear();
						old->Attach(nullptr, nullptr);
					}
					if (*holder)
						content->AddChild(std::move(*holder));
					content->InvalidateLayout();
				});
			return;
		}
		mChildren.clear();
		if (child)
			AddChild(std::move(child));
		InvalidateLayout();
	}
} // namespace Nuclear::UI
