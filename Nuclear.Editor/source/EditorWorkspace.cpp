#include <EditorWorkspace.h>
#include <EditorWidgets.h>
#include <Core/Engine.h>
#include <UI/CanvasPanel.h>
#include <cmath>
#include <stdexcept>

namespace Nuclear::Editor
{
	EditorWorkspace::EditorWorkspace(UI::UIContext& context, EditorTabStrip& documents, EditorDockArea& documentHost)
		: mContext(context), pDocuments(&documents), pDocumentHost(&documentHost), mDocumentsId(documents.GetId())
	{
		auto* window = Core::Engine::Get().GetMainWindow();
		documentHost.ShowHeader(false);
		Bind(documentHost, window, "");
		BindStrip(documents, documentHost, window);
		auto layout = std::make_unique<EditorDockLayout>();
		auto& viewport = mTabs["viewport"];
		viewport.Header = {"viewport", "Main viewport", false, false};
		viewport.Document = true;
		viewport.Detachable = false;
		viewport.OwnerDocument = "viewport";
		viewport.Layout = layout.get();
		viewport.Area = &documentHost;
		BindDocument("viewport", window);
		documentHost.AddTab(viewport.Header, std::move(layout));
	}

	EditorWorkspace::~EditorWorkspace() { Shutdown(); }

	void EditorWorkspace::Bind(EditorDockArea& area, Platform::Window* window, const std::string& document, bool floating)
	{
		mAreas[&area] = {area.GetId(), document, window, floating};
		area.OnChanged = [this]() { Refresh(); };
		BindStrip(area.GetStrip(), area, window);
	}

	void EditorWorkspace::BindDocument(const std::string& id, Platform::Window* window)
	{
		if (auto* layout = GetDocumentLayout(id))
			for (int index = 0; index < 5; ++index)
				Bind(layout->GetArea(static_cast<EditorDockRegion>(index)), window, id);
	}

	void EditorWorkspace::Unbind(EditorDockArea& area)
	{
		area.OnChanged = {};
		auto& strip = area.GetStrip();
		strip.OnSelected = {};
		strip.OnClosed = {};
		strip.OnDetached = {};
		strip.OnDragged = {};
		strip.OnDragCancelled = {};
	}

	void EditorWorkspace::BindStrip(EditorTabStrip& strip, EditorDockArea& area, Platform::Window* window)
	{
		strip.OnSelected = [&area](const std::string& id) { area.Activate(id); };
		strip.OnClosed = [this](const std::string& id) { Close(id); };
		strip.OnDetached = [this, window](const std::string& id) {
			if (window == Core::Engine::Get().GetMainWindow())
				Detach(id);
			else if (auto found = mTabs.find(id); found != mTabs.end())
				Dock(id, found->second.Home);
		};
		strip.OnDragged = [this, window](const std::string& id, UI::Vec2 point, bool dropped) {
			Drag(id, window, point, dropped);
		};
		strip.OnDragCancelled = [this]() { ClearPreviews(); };
	}

	bool EditorWorkspace::Contains(const std::string& id) const { return mTabs.contains(id); }

	EditorDockArea* EditorWorkspace::GetArea(const std::string& id) const
	{
		auto found = mTabs.find(id);
		return found == mTabs.end() ? nullptr : found->second.Area;
	}

	EditorDockLayout* EditorWorkspace::GetDocumentLayout(const std::string& id) const
	{
		auto found = mTabs.find(id);
		return found == mTabs.end() ? nullptr : found->second.Layout;
	}

	void EditorWorkspace::SetViewportContent(std::unique_ptr<UI::Widget> content)
	{
		auto& viewport = mTabs.at("viewport");
		viewport.Layout->GetArea(EditorDockRegion::Documents).AddTab(viewport.Header, std::move(content));
	}

	void EditorWorkspace::Add(const std::string& id, const std::string& title, std::unique_ptr<UI::Widget> content,
		EditorDockRegion home, bool document, bool closable, bool detachable, const std::string& ownerDocument)
	{
		if (!content || Contains(id))
			throw std::invalid_argument("Workspace tabs require unique identities and detached contents");
		if (!document && !GetDocumentLayout(ownerDocument))
			throw std::invalid_argument("A tool panel requires an existing owner document");

		auto& tab = mTabs[id];
		const auto caption = title.find_first_not_of(" \t\r\n") == std::string::npos ? "Untitled" : title;
		tab.Header = {id, caption, closable, detachable};
		tab.Home = home;
		tab.Document = document;
		tab.Detachable = detachable;
		tab.OwnerDocument = document ? id : ownerDocument;
		if (document)
		{
			auto layout = std::make_unique<EditorDockLayout>();
			tab.Layout = layout.get();
			BindDocument(id, Core::Engine::Get().GetMainWindow());
			tab.Layout->GetArea(EditorDockRegion::Documents).AddTab({id, caption, false, false}, std::move(content));
			content = std::move(layout);
		}
		tab.Area = &HomeArea(tab);
		tab.Area->AddTab(tab.Header, std::move(content));
	}

	EditorDockArea& EditorWorkspace::HomeArea(const TabState& tab) const
	{
		return tab.Document ? *pDocumentHost : GetDocumentLayout(tab.OwnerDocument)->GetArea(tab.Home);
	}

	void EditorWorkspace::SetTitle(const std::string& id, const std::string& title)
	{
		auto found = mTabs.find(id);
		if (found == mTabs.end())
			return;
		auto& tab = found->second;
		tab.Header.Title = title.find_first_not_of(" \t\r\n") == std::string::npos ? "Untitled" : title;
		if (tab.Area)
			tab.Area->SetTabTitle(id, tab.Header.Title);
		if (tab.Layout)
			tab.Layout->GetArea(EditorDockRegion::Documents).SetTabTitle(id, tab.Header.Title);
		for (auto& window : mWindows)
			if (&window->GetArea() == tab.Area && tab.Area->GetActive() == id)
				window->SetTitle("Nuclear / " + tab.Header.Title);
	}

	void EditorWorkspace::Refresh()
	{
		if (mMenu && mMenuDocument != pDocumentHost->GetActive())
			CloseMenu();
		pDocuments->SetTabs(pDocumentHost->GetTabs(), pDocumentHost->GetActive());
		for (auto& [id, tab] : mTabs)
		{
			if (!tab.Layout)
				continue;
			auto& center = tab.Layout->GetArea(EditorDockRegion::Documents);
			center.ShowHeader(center.GetTabs().size() > 1);
			tab.Layout->InvalidateLayout();
		}
	}

	void EditorWorkspace::ClearPreviews()
	{
		for (auto& [id, tab] : mTabs)
			if (tab.Layout)
				tab.Layout->SetDropPreview(-1);
	}

	void EditorWorkspace::MoveNow(const std::string& id, EditorDockArea& destination, size_t index)
	{
		auto found = mTabs.find(id);
		if (found == mTabs.end())
			return;
		auto& tab = found->second;
		auto target = mAreas.find(&destination);
		if (target == mAreas.end())
			return;
		if (!tab.Detachable && &destination != pDocumentHost)
			return;
		if (tab.Document && &destination != pDocumentHost && !target->second.Floating)
			return;
		if (&destination != pDocumentHost && target->second.Document != tab.OwnerDocument)
			return;
		if (!tab.Document && &destination == pDocumentHost)
			return;
		if (tab.Area == &destination && index == SIZE_MAX)
		{
			destination.Activate(id);
			return;
		}
		if (tab.Area == &destination)
		{
			const auto& tabs = destination.GetTabs();
			for (size_t old = 0; old < tabs.size(); ++old)
				if (tabs[old].Id == id && old < index)
				{
					--index;
					break;
				}
		}
		auto content = tab.Area ? tab.Area->TakeTab(id) : std::move(tab.ClosedContent);
		if (!content)
			return;
		tab.Area = &destination;
		if (tab.Document)
			BindDocument(id, target->second.Window);
		destination.AddTab(tab.Header, std::move(content), index);
	}

	void EditorWorkspace::ShowNow(const std::string& id)
	{
		auto found = mTabs.find(id);
		if (found == mTabs.end())
			return;
		auto& tab = found->second;
		if (!tab.Document)
			ShowNow(tab.OwnerDocument);
		if (!tab.Area)
			MoveNow(id, HomeArea(tab));
		else
			tab.Area->Activate(id);
		for (auto& window : mWindows)
			if (&window->GetArea() == tab.Area)
				window->GetWindow().Display(true);
	}

	void EditorWorkspace::Show(const std::string& id)
	{
		mPending.push_back([this, id]() { ShowNow(id); });
	}

	void EditorWorkspace::Close(const std::string& id)
	{
		mPending.push_back([this, id]() {
			auto found = mTabs.find(id);
			if (found == mTabs.end() || !found->second.Header.Closable || !found->second.Area)
				return;
			auto& tab = found->second;
			if (tab.Document)
				for (auto& [panelId, panel] : mTabs)
					if (!panel.Document && panel.OwnerDocument == id && panel.Area && mAreas.at(panel.Area).Floating)
						MoveNow(panelId, HomeArea(panel));
			tab.ClosedContent = tab.Area->TakeTab(id);
			tab.Area = nullptr;
		});
	}

	void EditorWorkspace::Dock(const std::string& id, EditorDockRegion region)
	{
		mPending.push_back([this, id, region]() {
			auto found = mTabs.find(id);
			if (found == mTabs.end())
				return;
			auto& tab = found->second;
			MoveNow(id, tab.Document ? *pDocumentHost : GetDocumentLayout(tab.OwnerDocument)->GetArea(region));
		});
	}

	void EditorWorkspace::Detach(const std::string& id)
	{
		mPending.push_back([this, id]() { DetachNow(id); });
	}

	void EditorWorkspace::DetachNow(const std::string& id)
	{
		auto found = mTabs.find(id);
		if (found == mTabs.end() || !found->second.Detachable)
			return;
		auto window = std::make_unique<EditorFloatingWindow>();
		if (!window->Initialize(found->second.Header.Title, mContext.GetDefaultFont()))
			return;
		auto* area = &window->GetArea();
		Bind(*area, &window->GetWindow(), found->second.OwnerDocument, true);
		area->SetEnabled(mEnabled);
		mWindows.push_back(std::move(window));
		MoveNow(id, *area);
	}

	void EditorWorkspace::Drag(const std::string& id, Platform::Window* origin, UI::Vec2 point, bool dropped)
	{
		if (!mEnabled || !origin)
			return;
		auto source = mTabs.find(id);
		if (source == mTabs.end())
			return;
		const auto& tab = source->second;
		auto* layout = GetDocumentLayout(pDocumentHost->GetActive());
		const auto* originContext = tab.Area ? tab.Area->GetContext() : nullptr;
		const float scale = originContext ? originContext->GetScale() : origin->GetDpiScale();
		int screenX = int(std::lround(point.x * scale));
		int screenY = int(std::lround(point.y * scale));
		if (!origin->ClientToScreen(screenX, screenY))
			return;
		int mainX = screenX;
		int mainY = screenY;
		Core::Engine::Get().GetMainWindow()->ScreenToClient(mainX, mainY);
		const UI::Vec2 main = {mainX / mContext.GetScale(), mainY / mContext.GetScale()};
		EditorDockArea* destination = nullptr;
		size_t insertion = SIZE_MAX;
		bool rejected = false;
		ClearPreviews();
		if (pDocuments->GetBounds().Contains(main))
		{
			if (tab.Document)
			{
				destination = pDocumentHost;
				insertion = pDocuments->InsertionAt(main);
			}
			else if (layout && tab.OwnerDocument == pDocumentHost->GetActive())
				destination = &layout->GetArea(EditorDockRegion::Documents);
			else
				rejected = true;
		}
		else if (layout && layout->GetBounds().Contains(main))
		{
			if (tab.Document)
				destination = pDocumentHost;
			else if (tab.OwnerDocument == pDocumentHost->GetActive())
			{
				const auto region = layout->DropRegion(main);
				destination = &layout->GetArea(region);
				layout->SetDropPreview(static_cast<int>(region));
				if (destination->GetStrip().GetBounds().Contains(main))
					insertion = destination->GetStrip().InsertionAt(main);
			}
			else
				rejected = true;
		}
		else
		{
			for (auto& window : mWindows)
			{
				int x = screenX;
				int y = screenY;
				window->GetWindow().ScreenToClient(x, y);
				const float targetScale = window->GetContext().GetScale();
				const UI::Vec2 local = {x / targetScale, y / targetScale};
				if (!window->GetArea().GetBounds().Contains(local))
					continue;
				destination = &window->GetArea();
				if (mAreas.at(destination).Document != tab.OwnerDocument)
				{
					destination = nullptr;
					rejected = true;
					break;
				}
				insertion = destination->GetStrip().InsertionAt(local);
				break;
			}
		}
		if (!dropped || rejected)
			return;
		ClearPreviews();
		if (destination)
			mPending.push_back([this, id, destination, insertion]() { MoveNow(id, *destination, insertion); });
		else
			Detach(id);
	}

	void EditorWorkspace::ResetLayout()
	{
		const auto owner = pDocumentHost->GetActive();
		mPending.push_back([this, owner]() {
			auto* layout = GetDocumentLayout(owner);
			if (!layout)
				return;
			for (auto& [id, tab] : mTabs)
				if (!tab.Document && tab.OwnerDocument == owner)
					MoveNow(id, HomeArea(tab));
			layout->ResetSplits();
			layout->GetArea(EditorDockRegion::Documents).Activate(owner);
		});
	}

	void EditorWorkspace::CloseMenu()
	{
		if (mMenu)
			mContext.RemoveFromViewport(std::exchange(mMenu, 0));
	}

	void EditorWorkspace::ToggleWindowMenu()
	{
		if (mMenu)
		{
			CloseMenu();
			return;
		}
		auto root = std::make_unique<UI::CanvasPanel>();
		auto& border = root->Add<UI::Border>();
		border.SetBrush({nullptr, {.065f, .085f, .10f, 1}});
		border.SetPadding({8, 8, 8, 8});
		UI::Slot slot;
		slot.Offsets = {760, 135, 300, 400};
		root->SetSlot(border.GetId(), slot);
		auto& list = border.Add<UI::VerticalBox>();
		list.SetSpacing(5);
		Label(list, "WINDOW / LAYOUT", 14);
		Label(list, "Drag tabs to dock or float. The window icon pops out or returns a tab.", 12).SetWrap(true);
		for (auto& [id, tab] : mTabs)
		{
			if (tab.Document || tab.OwnerDocument != pDocumentHost->GetActive())
				continue;
			Action(list, "Show " + tab.Header.Title, [this, id]() {
				Show(id);
				CloseMenu();
			});
		}
		Action(list, "Reset layout", [this]() {
			ResetLayout();
			CloseMenu();
		});
		Action(list, "Close menu", [this]() { CloseMenu(); });
		mMenu = root->GetId();
		mMenuDocument = pDocumentHost->GetActive();
		mContext.AddToViewport(std::move(root), 50);
	}

	void EditorWorkspace::SetInteractionEnabled(bool enabled)
	{
		if (enabled == mEnabled)
			return;
		mEnabled = enabled;
		if (!enabled)
			CloseMenu();
		for (auto& window : mWindows)
			window->GetArea().SetEnabled(enabled);
	}

	void EditorWorkspace::Update()
	{
		auto commands = std::exchange(mPending, {});
		for (auto& command : commands)
			command();
		std::erase_if(mWindows, [this](const auto& window) {
			if (!window->GetArea().GetTabs().empty())
				return false;
			mAreas.erase(&window->GetArea());
			return true;
		});
	}

	void EditorWorkspace::RenderWindows()
	{
		for (auto& window : mWindows)
		{
			window->Render();
			window->Present();
		}
	}

	void EditorWorkspace::ProcessEvent(const Platform::Event& event)
	{
		for (auto& window : mWindows)
		{
			if (event.pWindow != &window->GetWindow())
				continue;
			if (event.mType == Platform::EventType::WindowClose)
			{
				for (const auto& tab : window->GetArea().GetTabs())
					Dock(tab.Id, mTabs.at(tab.Id).Home);
			}
			else
				window->ProcessEvent(event);
			break;
		}
	}

	void EditorWorkspace::Shutdown()
	{
		mPending.clear();
		CloseMenu();
		for (auto& [area, binding] : mAreas)
		{
			bool alive = mContext.Find(binding.Id) == area;
			for (const auto& window : mWindows)
				alive = alive || window->GetContext().Find(binding.Id) == area;
			const auto owner = mTabs.find(binding.Document);
			if (!binding.Floating && owner != mTabs.end() && owner->second.ClosedContent)
				alive = true;
			if (alive)
				Unbind(*area);
		}
		if (mContext.Find(mDocumentsId) == pDocuments)
		{
			pDocuments->OnSelected = {};
			pDocuments->OnClosed = {};
			pDocuments->OnDetached = {};
			pDocuments->OnDragged = {};
			pDocuments->OnDragCancelled = {};
		}
		mWindows.clear();
		mTabs.clear();
		mAreas.clear();
		mMenu = 0;
	}
}
