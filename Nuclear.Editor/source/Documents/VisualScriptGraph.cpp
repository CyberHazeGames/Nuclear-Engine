#include <VisualScriptGraph.h>
#include <Assimp/contrib/rapidjson/include/rapidjson/document.h>
#include <Assimp/contrib/rapidjson/include/rapidjson/prettywriter.h>
#include <Assimp/contrib/rapidjson/include/rapidjson/stringbuffer.h>
#include <cmath>

namespace Nuclear::Editor
{
	namespace
	{
		using Graph = VisualScriptGraph;
		constexpr size_t MaxNodes = 256;
		constexpr Graph::Kind Kinds[] = {Graph::Kind::Begin, Graph::Kind::Branch, Graph::Kind::Print, Graph::Kind::Boolean};
		constexpr Graph::Pin BeginPins[] = {{"Then", Graph::Type::Flow, true, 0}};
		constexpr Graph::Pin BranchPins[] = {
			{"In", Graph::Type::Flow, false, 0}, {"Condition", Graph::Type::Boolean, false, 1},
			{"True", Graph::Type::Flow, true, 0}, {"False", Graph::Type::Flow, true, 1}};
		constexpr Graph::Pin PrintPins[] = {{"In", Graph::Type::Flow, false, 0}, {"Then", Graph::Type::Flow, true, 0}};
		constexpr Graph::Pin BooleanPins[] = {{"Value", Graph::Type::Boolean, true, 0}};

		std::string_view String(const rapidjson::Value& value)
		{
			return value.IsString() ? std::string_view(value.GetString(), value.GetStringLength()) : std::string_view{};
		}

		bool ReadEndpoint(const rapidjson::Value& data, Graph::Endpoint& endpoint)
		{
			if (!data.IsArray() || data.Size() != 2 || !data[0].IsInt() || !data[1].IsString()) return false;
			endpoint = {data[0].GetInt(), std::string(String(data[1]))};
			return true;
		}
	}

	const char* VisualScriptGraph::Title(Kind kind)
	{
		switch (kind)
		{
		case Kind::Begin: return "Begin Play";
		case Kind::Branch: return "Branch";
		case Kind::Print: return "Print Text";
		case Kind::Boolean: return "Boolean";
		}
		return "";
	}

	std::span<const VisualScriptGraph::Pin> VisualScriptGraph::Pins(Kind kind)
	{
		switch (kind)
		{
		case Kind::Begin: return BeginPins;
		case Kind::Branch: return BranchPins;
		case Kind::Print: return PrintPins;
		case Kind::Boolean: return BooleanPins;
		}
		return {};
	}

	VisualScriptGraph VisualScriptGraph::Example()
	{
		VisualScriptGraph graph;
		int begin = graph.Add(Kind::Begin, {32, 52});
		int condition = graph.Add(Kind::Boolean, {32, 204});
		int branch = graph.Add(Kind::Branch, {252, 112});
		int print = graph.Add(Kind::Print, {472, 52});
		graph.Connect({begin, "Then"}, {branch, "In"});
		graph.Connect({condition, "Value"}, {branch, "Condition"});
		graph.Connect({branch, "True"}, {print, "In"});
		return graph;
	}

	int VisualScriptGraph::Add(Kind kind, UI::Vec2 position)
	{
		if (mNodes.size() >= MaxNodes) return 0;
		int id = 1;
		while (mNodes.contains(id)) ++id;
		mNodes.emplace(id, Node{kind, position});
		return id;
	}

	void VisualScriptGraph::Remove(int id)
	{
		std::erase_if(mLinks, [id](const Link& link) { return link.From.Id == id || link.To.Id == id; });
		mNodes.erase(id);
	}

	VisualScriptGraph::Node* VisualScriptGraph::Find(int id)
	{
		auto it = mNodes.find(id);
		return it == mNodes.end() ? nullptr : &it->second;
	}

	const VisualScriptGraph::Pin* VisualScriptGraph::FindPin(const Endpoint& endpoint) const
	{
		auto node = mNodes.find(endpoint.Id);
		if (node == mNodes.end()) return nullptr;
		for (const auto& pin : Pins(node->second.NodeKind))
			if (endpoint.PinName == pin.Name) return &pin;
		return nullptr;
	}

	void VisualScriptGraph::Disconnect(const Endpoint& endpoint)
	{
		std::erase_if(mLinks, [&](const Link& link) { return link.To == endpoint || link.From == endpoint; });
	}

	bool VisualScriptGraph::Connect(Endpoint from, Endpoint to)
	{
		const auto* output = FindPin(from);
		const auto* input = FindPin(to);
		if (!output || !input || from.Id == to.Id || !output->Output || input->Output || output->DataType != input->DataType)
			return false;
		Disconnect(to);
		mLinks.push_back({std::move(from), std::move(to)});
		return true;
	}

	std::string VisualScriptGraph::ToJson() const
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		writer.SetFormatOptions(rapidjson::kFormatSingleLineArray);
		writer.StartObject();
		writer.Key("version");
		writer.Int(1);
		writer.Key("nodes");
		writer.StartArray();
		for (const auto& [id, node] : mNodes)
		{
			writer.StartObject();
			writer.Key("id");
			writer.Int(id);
			writer.Key("kind");
			writer.String(Title(node.NodeKind));
			writer.Key("position");
			writer.StartArray();
			writer.Double(node.Position.x);
			writer.Double(node.Position.y);
			writer.EndArray();
			if (node.NodeKind == Kind::Print)
			{
				writer.Key("message");
				writer.String(node.Message.data(), static_cast<rapidjson::SizeType>(node.Message.size()));
			}
			if (node.NodeKind == Kind::Boolean)
			{
				writer.Key("value");
				writer.Bool(node.Value);
			}
			writer.EndObject();
		}
		writer.EndArray();
		writer.Key("links");
		writer.StartArray();
		for (const auto& link : mLinks)
		{
			writer.StartObject();
			auto endpoint = [&](const char* key, const Endpoint& pin) {
				writer.Key(key);
				writer.StartArray();
				writer.Int(pin.Id);
				writer.String(pin.PinName.c_str());
				writer.EndArray();
			};
			endpoint("from", link.From);
			endpoint("to", link.To);
			writer.EndObject();
		}
		writer.EndArray();
		writer.EndObject();
		return {buffer.GetString(), buffer.GetSize()};
	}

	bool VisualScriptGraph::LoadJson(const std::string& json)
	{
		if (json.size() > 8 * 1024 * 1024 || json.find('\0') != std::string::npos) return false;
		rapidjson::Document data;
		data.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(json.data(), json.size());
		if (!data.IsObject() || !data.HasMember("version") || !data["version"].IsInt() || data["version"].GetInt() != 1 ||
			!data.HasMember("nodes") || !data["nodes"].IsArray() || data["nodes"].Size() > MaxNodes ||
			!data.HasMember("links") || !data["links"].IsArray() || data["links"].Size() > MaxNodes * 2)
			return false;

		VisualScriptGraph next;
		for (const auto& entry : data["nodes"].GetArray())
		{
			if (!entry.IsObject() || !entry.HasMember("id") || !entry["id"].IsInt() || entry["id"].GetInt() <= 0 ||
				!entry.HasMember("kind") || !entry.HasMember("position")) return false;
			const auto kind = std::ranges::find_if(Kinds, [&](Kind k) { return Title(k) == String(entry["kind"]); });
			const auto& pos = entry["position"];
			if (kind == std::end(Kinds) || !pos.IsArray() || pos.Size() != 2) return false;
			for (const auto& axis : pos.GetArray())
				if (!axis.IsNumber() || std::abs(axis.GetDouble()) > 1000000) return false;
			Node node{*kind, {pos[0].GetFloat(), pos[1].GetFloat()}};
			if (*kind == Kind::Print)
			{
				if (!entry.HasMember("message") || !entry["message"].IsString() || entry["message"].GetStringLength() > 4096)
					return false;
				node.Message = String(entry["message"]);
			}
			if (*kind == Kind::Boolean)
			{
				if (!entry.HasMember("value") || !entry["value"].IsBool()) return false;
				node.Value = entry["value"].GetBool();
			}
			if (!next.mNodes.emplace(entry["id"].GetInt(), std::move(node)).second) return false;
		}
		for (const auto& entry : data["links"].GetArray())
		{
			Endpoint from, to;
			if (!entry.IsObject() || !entry.HasMember("from") || !entry.HasMember("to") ||
				!ReadEndpoint(entry["from"], from) || !ReadEndpoint(entry["to"], to)) return false;
			const size_t previous = next.mLinks.size();
			if (!next.Connect(std::move(from), std::move(to)) || next.mLinks.size() == previous) return false;
		}
		*this = std::move(next);
		return true;
	}
}
