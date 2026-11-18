#pragma once

#include <UI/Types.h>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Nuclear::Editor
{
	/// Editable document data for the visual scripting canvas. All positions are in graph units.
	class VisualScriptGraph
	{
	public:
		/// Built-in palette entries with fixed, named pins.
		enum class Kind { Begin, Branch, Print, Boolean };
		/// Flow connections and Boolean values use separate pin types.
		enum class Type { Flow, Boolean };

		/// A pin name is stable within its node kind and is also its serialized identifier.
		struct Pin
		{
			const char* Name; ///< Visible and serialized pin name.
			Type DataType; ///< Required type for a connection.
			bool Output; ///< Outputs connect to inputs.
			int Row; ///< Zero-based vertical slot on this side of the card.
		};

		/// One card and its editable literal, when applicable.
		struct Node
		{
			Kind NodeKind; ///< Determines the available pins and properties.
			UI::Vec2 Position; ///< Top-left graph coordinate.
			std::string Message = "Hello, world!"; ///< Text literal on Print Text nodes.
			bool Value = true; ///< Literal output on Boolean nodes.
		};

		/// A node ID and one of its named pins. Zero denotes no endpoint.
		struct Endpoint
		{
			int Id = 0; ///< Document-local node identity.
			std::string PinName; ///< Stable name in the node kind's pin list.
			bool operator==(const Endpoint&) const = default; ///< Compares both node and pin.
		};

		/// A directed wire between compatible pins.
		struct Link
		{
			Endpoint From; ///< Output endpoint.
			Endpoint To; ///< Input endpoint.
		};

		static const char* Title(Kind kind); ///< Returns the palette title for a supported kind.
		static std::span<const Pin> Pins(Kind kind); ///< Borrows the immutable pin layout.
		static VisualScriptGraph Example(); ///< Builds the initially displayed branching graph.
		int Add(Kind kind, UI::Vec2 position); ///< Returns a free positive ID, or zero at capacity.
		void Remove(int id); ///< Removes the card and all incident wires.
		Node* Find(int id); ///< Borrows a card until deletion, or null for a missing ID.
		const Pin* FindPin(const Endpoint& endpoint) const; ///< Returns null for an unknown node or pin.
		bool Connect(Endpoint from, Endpoint to); ///< Connects compatible pins, replacing the input's previous wire.
		void Disconnect(const Endpoint& endpoint); ///< Removes wires touching this pin.
		std::string ToJson() const; ///< Returns versioned UTF-8 JSON with graph coordinates and literals.
		bool LoadJson(const std::string& json); ///< Replaces the document only after validating the entire graph.
		const auto& Nodes() const { return mNodes; } ///< Borrows nodes in stable painting order.
		const auto& Links() const { return mLinks; } ///< Borrows the document's directed wires.

	private:
		std::map<int, Node> mNodes; ///< Positive IDs remain stable across moves and serialization.
		std::vector<Link> mLinks; ///< Each input has at most one source.
	};
}
