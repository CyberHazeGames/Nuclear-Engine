#include <VisualScriptCanvas.h>
#include <EditorSettings.h>
#include <Nuclear.Platform/Input.h>
#include <UI/DrawList.h>
#include <cmath>

namespace Nuclear::Editor
{
	namespace
	{
		using Graph = VisualScriptGraph;
		using namespace UI;
		constexpr float CardWidth = 170;
		constexpr float CoordinateLimit = 1000000;

		float Height(Graph::Kind kind)
		{
			return kind == Graph::Kind::Begin ? 66.f : 90.f;
		}

		Vec2 PinPosition(const Graph::Node& node, const Graph::Pin& pin)
		{
			return {node.Position.x + (pin.Output ? CardWidth : 0), node.Position.y + 46 + 22 * pin.Row};
		}

		Color HeaderColor(Graph::Kind kind)
		{
			switch (kind)
			{
			case Graph::Kind::Begin: return {.43f, .10f, .13f, 1};
			case Graph::Kind::Branch: return {.26f, .19f, .38f, 1};
			case Graph::Kind::Print: return {.08f, .25f, .41f, 1};
			case Graph::Kind::Boolean: return {.08f, .30f, .25f, 1};
			}
			return {};
		}

		Color PinColor(Graph::Type type)
		{
			return type == Graph::Type::Flow ? Color{.83f, .85f, .91f, 1} : Color{.30f, .86f, .59f, 1};
		}

		void Wire(DrawList& list, Rect clip, Vec2 from, Vec2 to, Color color, float zoom)
		{
			const float width = std::max(1.f, 2 * zoom);
			auto segment = [&](Vec2 a, Vec2 b) {
				list.Add({std::min(a.x, b.x) - width / 2, std::min(a.y, b.y) - width / 2,
					std::abs(b.x - a.x) + width, std::abs(b.y - a.y) + width}, clip, color);
			};
			if (to.x >= from.x + 22 * zoom)
			{
				const float mid = (to.x + from.x) / 2;
				segment(from, {mid, from.y});
				segment({mid, from.y}, {mid, to.y});
				segment({mid, to.y}, to);
				return;
			}
			// Approach the input from the left even when the destination is behind the source.
			const float right = from.x + 20 * zoom, left = to.x - 20 * zoom;
			const float lane = std::max(from.y, to.y) + 58 * zoom;
			segment(from, {right, from.y});
			segment({right, from.y}, {right, lane});
			segment({right, lane}, {left, lane});
			segment({left, lane}, {left, to.y});
			segment({left, to.y}, to);
		}
	}

	UI::Vec2 VisualScriptCanvas::Screen(UI::Vec2 point) const
	{
		const auto bounds = GetBounds();
		return {bounds.x + mPan.x + point.x * mZoom, bounds.y + mPan.y + point.y * mZoom};
	}

	UI::Vec2 VisualScriptCanvas::World(UI::Vec2 point) const
	{
		const auto origin = Screen({});
		return {(point.x - origin.x) / mZoom, (point.y - origin.y) / mZoom};
	}

	VisualScriptGraph::Node* VisualScriptCanvas::Selected() { return mGraph.Find(mSelected); }

	void VisualScriptCanvas::Select(int id)
	{
		mSelected = id;
		if (OnNotice) OnNotice("");
		if (OnSelection) OnSelection();
	}

	void VisualScriptCanvas::AddNode(Graph::Kind kind)
	{
		const auto bounds = GetBounds();
		const float stagger = 16.f * (mGraph.Nodes().size() % 5);
		auto position = World({bounds.x + bounds.Width / 2, bounds.y + bounds.Height / 2});
		position.x = std::clamp(position.x - CardWidth / 2 + stagger, -CoordinateLimit, CoordinateLimit);
		position.y = std::clamp(position.y - Height(kind) / 2 + stagger, -CoordinateLimit, CoordinateLimit);
		const int id = mGraph.Add(kind, position);
		if (id) Select(id);
		else if (OnNotice) OnNotice("Graph limit reached (256 nodes).");
	}

	void VisualScriptCanvas::DeleteSelected()
	{
		CancelDrag();
		mGraph.Remove(mSelected);
		Select(0);
	}

	std::string VisualScriptCanvas::ToJson() const { return mGraph.ToJson(); }

	bool VisualScriptCanvas::LoadJson(const std::string& text)
	{
		if (!mGraph.LoadJson(text)) return false;
		mDrag = Drag::None;
		Select(0);
		FrameGraph();
		return true;
	}

	void VisualScriptCanvas::OnArrange(UI::Rect content)
	{
		if (mNeedsInitialFrame && content.Width > 0 && content.Height > 0)
		{
			mNeedsInitialFrame = false;
			FrameGraph();
		}
	}

	void VisualScriptCanvas::FrameGraph()
	{
		const auto bounds = GetBounds();
		if (mGraph.Nodes().empty() || bounds.Width <= 0 || bounds.Height <= 0)
		{
			mPan = {};
			mZoom = 1;
			return;
		}
		UI::Vec2 low{CoordinateLimit, CoordinateLimit}, high{-CoordinateLimit, -CoordinateLimit};
		for (const auto& [id, node] : mGraph.Nodes())
		{
			low.x = std::min(low.x, node.Position.x);
			low.y = std::min(low.y, node.Position.y);
			high.x = std::max(high.x, node.Position.x + CardWidth);
			high.y = std::max(high.y, node.Position.y + Height(node.NodeKind));
		}
		mZoom = std::clamp(std::min((bounds.Width - 40) / (high.x - low.x),
			(bounds.Height - 40) / (high.y - low.y)), .35f, 1.f);
		mPan = {(bounds.Width - (high.x + low.x) * mZoom) / 2, (bounds.Height - (high.y + low.y) * mZoom) / 2};
	}

	int VisualScriptCanvas::HitNode(UI::Vec2 world) const
	{
		auto contains = [&](const Graph::Node& node) {
			return UI::Rect{node.Position.x - 5, node.Position.y, CardWidth + 10, Height(node.NodeKind)}.Contains(world);
		};
		const auto& nodes = mGraph.Nodes();
		if (nodes.contains(mSelected) && contains(nodes.at(mSelected))) return mSelected;
		for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
			if (contains(it->second)) return it->first;
		return 0;
	}

	VisualScriptGraph::Endpoint VisualScriptCanvas::HitPin(int id, UI::Vec2 world) const
	{
		if (!id) return {};
		const auto& node = mGraph.Nodes().at(id);
		for (const auto& pin : Graph::Pins(node.NodeKind))
		{
			const auto at = PinPosition(node, pin);
			if (std::abs(world.x - at.x) <= 10 && std::abs(world.y - at.y) <= 10) return {id, pin.Name};
		}
		return {};
	}

	void VisualScriptCanvas::CancelDrag()
	{
		if (mDrag == Drag::Move && Selected()) Selected()->Position = mOriginal;
		if (mDrag == Drag::Pan) mPan = mOriginal;
		mDrag = Drag::None;
	}

	void VisualScriptCanvas::OnRemoved() { CancelDrag(); }

	UI::EventReply VisualScriptCanvas::OnInput(const UI::InputEvent& event)
	{
		using Type = UI::InputEvent::Type;
		using Key = Platform::Input;
		if (event.EventType == Type::Cancel || (event.EventType == Type::KeyDown && event.Key == Key::KEYCODE_ESCAPE))
		{
			CancelDrag();
			return {true, false, true};
		}
		if (event.EventType == Type::KeyDown && event.Key == Key::KEYCODE_DELETE)
		{
			DeleteSelected();
			return {true, false, true};
		}
		const auto world = World(event.Position);
		if (event.EventType == Type::Wheel && mDrag == Drag::None)
		{
			mZoom = std::clamp(mZoom * std::pow(1.15f, event.WheelY), .35f, 2.f);
			const auto after = Screen(world);
			mPan.x += event.Position.x - after.x;
			mPan.y += event.Position.y - after.y;
			return {true};
		}
		if (event.EventType == Type::PointerDown && mDrag == Drag::None)
		{
			const int id = HitNode(world);
			auto pin = HitPin(id, world);
			if (event.Button == MOUSE_BUTTON_RIGHT)
			{
				if (pin.Id) mGraph.Disconnect(pin);
				return {true, false, false, true};
			}
			if (event.Button != MOUSE_BUTTON_LEFT && event.Button != MOUSE_BUTTON_MIDDLE) return {};
			mButton = event.Button;
			mStart = mPointer = event.Position;
			if (event.Button == MOUSE_BUTTON_LEFT) Select(id);
			if (event.Button == MOUSE_BUTTON_MIDDLE || !id)
			{
				mDrag = Drag::Pan;
				mOriginal = mPan;
			}
			else if (pin.Id)
			{
				mDrag = Drag::Wire;
				mWire = std::move(pin);
			}
			else
			{
				mDrag = Drag::Move;
				mOriginal = Selected()->Position;
			}
			return {true, true, false, true};
		}
		if (mDrag == Drag::None) return {};
		const bool released = event.EventType == Type::PointerUp && event.Button == mButton;
		if (event.EventType != Type::PointerMove && !released) return {};
		mPointer = event.Position;
		const UI::Vec2 delta{mPointer.x - mStart.x, mPointer.y - mStart.y};
		if (mDrag == Drag::Pan) mPan = {mOriginal.x + delta.x, mOriginal.y + delta.y};
		if (mDrag == Drag::Move && Selected())
			Selected()->Position = {std::clamp(mOriginal.x + delta.x / mZoom, -CoordinateLimit, CoordinateLimit),
				std::clamp(mOriginal.y + delta.y / mZoom, -CoordinateLimit, CoordinateLimit)};
		if (released && mDrag == Drag::Wire && GetBounds().Contains(event.Position))
		{
			auto target = HitPin(HitNode(world), world);
			if (target.Id)
			{
				auto source = mWire;
				if (!mGraph.FindPin(source)->Output) std::swap(source, target);
				if (!mGraph.Connect(source, target) && OnNotice) OnNotice("Connect matching types from an output to an input.");
			}
		}
		if (released) mDrag = Drag::None;
		return {true, false, released};
	}

	void VisualScriptCanvas::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		using namespace UI;
		const auto bounds = GetBounds();
		list.Add(bounds, clip, {.035f, .046f, .058f, 1});
		const float step = 32 * mZoom;
		const Color grid{.068f, .083f, .10f, 1};
		for (float x = bounds.x + std::fmod(mPan.x, step); x < bounds.x + bounds.Width; x += step)
			list.Add({x, bounds.y, 1, bounds.Height}, clip, grid);
		for (float y = bounds.y + std::fmod(mPan.y, step); y < bounds.y + bounds.Height; y += step)
			list.Add({bounds.x, y, bounds.Width, 1}, clip, grid);

		auto anchor = [&](const Graph::Endpoint& end) {
			return Screen(PinPosition(mGraph.Nodes().at(end.Id), *mGraph.FindPin(end)));
		};
		for (const auto& link : mGraph.Links())
			Wire(list, clip, anchor(link.From), anchor(link.To), PinColor(mGraph.FindPin(link.From)->DataType), mZoom);
		if (mDrag == Drag::Wire)
		{
			auto from = anchor(mWire), to = mPointer;
			const auto* pin = mGraph.FindPin(mWire);
			if (!pin->Output) std::swap(from, to);
			Wire(list, clip, from, to, PinColor(pin->DataType), mZoom);
		}

		const auto* font = ResolveFont(nullptr);
		auto drawNode = [&](int id, const Graph::Node& node) {
			const auto origin = Screen(node.Position);
			auto rect = [&](float x, float y, float w, float h) {
				return Rect{origin.x + x * mZoom, origin.y + y * mZoom, w * mZoom, h * mZoom};
			};
			const float height = Height(node.NodeKind);
			const auto card = rect(0, 0, CardWidth, height);
			const auto ink = card.Intersect(clip);
			if (ink.Width <= 0 || ink.Height <= 0) return;
			list.Add(rect(3, 4, CardWidth, height), clip, {0, 0, 0, .35f});
			list.Add(rect(-1, -1, CardWidth + 2, height + 2), clip,
				id == mSelected ? EditorSettings::Get().GetAccentColor() : Color{.20f, .24f, .27f, 1});
			list.Add(card, ink, {.087f, .105f, .13f, 1});
			list.Add(rect(0, 0, CardWidth, 30), ink, HeaderColor(node.NodeKind));
			list.Text(font, Graph::Title(node.NodeKind), rect(12, 5, CardWidth - 20, 24), ink, 16 * mZoom, {}, Alignment::Start, false);
			for (const auto& pin : Graph::Pins(node.NodeKind))
			{
				const auto p = Screen(PinPosition(node, pin));
				const auto tint = PinColor(pin.DataType);
				list.Add({p.x - 4 * mZoom, p.y - 4 * mZoom, 8 * mZoom, 8 * mZoom}, clip, tint);
				const auto size = 13 * mZoom;
				const float labelX = pin.Output ? CardWidth - 18 - DrawList::MeasureText(font, pin.Name, size).x / mZoom : 18;
				list.Text(font, pin.Name, rect(labelX, 37.f + 22.f * pin.Row, CardWidth - 28, 20), ink, size, tint, Alignment::Start, false);
			}
			if (node.NodeKind == Graph::Kind::Print || node.NodeKind == Graph::Kind::Boolean)
			{
				auto field = rect(10, 62, CardWidth - 20, 22);
				list.Add(field, ink, {.052f, .066f, .081f, 1});
				const std::string_view value = node.NodeKind == Graph::Kind::Print ? std::string_view(node.Message) :
					(node.Value ? "true" : "false");
				list.Text(font, value, rect(16, 65, CardWidth - 32, 18), field.Intersect(ink),
					13 * mZoom, {.73f, .79f, .84f, 1}, Alignment::Start, false);
			}
		};
		for (const auto& [id, node] : mGraph.Nodes())
			if (id != mSelected) drawNode(id, node);
		if (mSelected) drawNode(mSelected, mGraph.Nodes().at(mSelected));
	}
}
