#include <ProjectDocument.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <algorithm>
#include <cmath>

namespace Nuclear::Editor
{
	namespace
	{
		void Expect(std::istream& in, const char* expected)
		{
			std::string token;
			if (!(in >> token) || token != expected) throw std::runtime_error(std::string("Expected ") + expected);
		}
		template<class T> T Number(std::istream& in, T low, T high)
		{
			T value{};
			if (!(in >> value) || !std::isfinite(double(value)) || value < low || value > high)
				throw std::runtime_error("Invalid project number");
			return value;
		}
		float Scalar(std::istream& in)
		{
			return Number(in, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		}
		bool Flag(std::istream& in) { return Number(in, 0, 1) != 0; }
		std::string String(std::istream& in)
		{
			std::string value;
			if (!(in >> std::ws) || in.peek() != '"' || !(in >> std::quoted(value)) || value.find('\0') != value.npos)
				throw std::runtime_error("Invalid project string");
			return value;
		}
		template<class V> V Vector(std::istream& in)
		{
			V value{};
			for (int i = 0; i < value.length(); ++i) value[i] = Scalar(in);
			return value;
		}
		template<class V> void WriteVector(std::ostream& out, const V& value)
		{
			for (int i = 0; i < value.length(); ++i) out << ' ' << value[i];
		}
		std::string Relative(const std::filesystem::path& path, const std::filesystem::path& base)
		{
			if (path.empty()) return {};
			auto relative = std::filesystem::absolute(path).lexically_normal().lexically_relative(base);
			if (relative.empty()) return std::filesystem::absolute(path).generic_string();
			return relative.generic_string();
		}
	}
	bool ProjectDocument::IsMap(const std::filesystem::path& file)
	{
		return _wcsicmp(file.extension().c_str(), L".NMap") == 0;
	}
	bool ProjectDocument::IsProject(const std::filesystem::path& file)
	{
		return _wcsicmp(file.extension().c_str(), L".nuclearproject") == 0;
	}
	ProjectDocument ProjectDocument::Capture(Project& project, entt::entity preview, entt::entity gameCamera)
	{
		ProjectDocument doc;
		doc.ProjectName = project.GetProjectInfo().mProjectName;
		auto& scene = Core::Scene::Get();
		doc.SceneName = scene.GetSceneAsset()->GetName();
		doc.ClearColor = scene.GetMainCamera()->mRTClearColor.Get4f();
		for (auto entity : scene.GetRegistry().view<Components::EntityInfoComponent>())
		{
			if (entity == preview) continue;
			auto state = EditorEntityState::Capture(entity);
			if (entity == gameCamera && state.mCamera) doc.mCamera = static_cast<int>(doc.mEntities.size());
			doc.mEntities.push_back(std::move(state));
		}
        std::set<std::string> ids;
        for (const auto& entity : doc.mEntities) ids.insert(entity.mId);
        for (auto& entity : doc.mEntities)
            if (!ids.contains(entity.mParent)) entity.mParent.clear();
        return doc;
    }
    ProjectDocument ProjectDocument::CapturePrefab(entt::entity entity)
    {
        auto state = EditorEntityState::Capture(entity);
        if (!state.Exists) throw std::runtime_error("Select an entity to save as a prefab");
        state.mParent.clear();
        ProjectDocument result;
        result.ProjectName = result.SceneName = state.GetName();
        result.mEntities.push_back(std::move(state));
        return result;
    }
    EditorEntityState ProjectDocument::Template() const
    {
        if (mEntities.size() != 1) throw std::runtime_error("A prefab must contain one entity");
        return mEntities.front();
    }
    void ProjectDocument::Save(const std::filesystem::path& file, const Project& project) const
	{
		Write(file, &project);
	}
	void ProjectDocument::SaveManifest(const std::filesystem::path& file) const
	{
		if (!IsProject(file) || !IsMap(StartupMap) || !std::filesystem::is_regular_file(StartupMap))
			throw std::runtime_error("A .nuclearproject must reference an existing startup .NMap");
		const auto base = std::filesystem::absolute(file).parent_path();
		const auto temporary = base / (Core::UUID::CreateNewUUID().str() + ".tmp");
		try
		{
			std::ofstream out(temporary, std::ios::binary);
			out.exceptions(std::ios::failbit | std::ios::badbit);
			out << "NuclearProject 3\nproject " << std::quoted(ProjectName)
				<< "\nassembly " << std::quoted(Relative(Assembly, base)) << "\nlevels " << Levels.size();
			for (const auto& [name, path] : Levels)
			{
				if (!IsMap(path)) throw std::runtime_error("Linked maps must use .NMap");
				out << "\nlevel " << std::quoted(name) << ' ' << std::quoted(Relative(path, base));
			}
			out << "\nprefabs " << Prefabs.size();
			for (const auto& [name, path] : Prefabs)
				out << "\nprefab " << std::quoted(name) << ' ' << std::quoted(Relative(path, base));
			out << "\nmap " << std::quoted(Relative(StartupMap, base)) << "\nend\n";
			out.close();
			std::filesystem::rename(temporary, file);
		}
		catch (...)
		{
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			throw;
		}
	}

	void ProjectDocument::ConvertLegacyLevels(const std::filesystem::path& directory)
	{
		auto convert = [&](std::filesystem::path& path) {
			if (path.empty() || IsMap(path)) return;
			if (!IsProject(path)) throw std::runtime_error("Invalid legacy level file: " + path.string());
			auto legacy = Read(path);
			if (!legacy.Assembly.empty() && legacy.Assembly != Assembly)
				throw std::runtime_error("Legacy level uses a different gameplay assembly: " + path.string());
			auto target = directory / path.filename().replace_extension(".NMap");
			if (std::filesystem::exists(target))
				target = directory / (path.stem().string() + "-" + Core::UUID::CreateNewUUID().str() + ".NMap");
			for (auto& [mesh, material] : legacy.mReferences)
				for (auto* reference : {&mesh, &material})
					if (reference->starts_with("file:"))
						*reference = "file:" + Relative(legacy.mDirectory / reference->substr(5), directory);
			legacy.Write(target, nullptr);
			path = target;
		};
		convert(StartupMap);
		for (auto& [name, path] : Levels) convert(path);
	}
	void ProjectDocument::Write(const std::filesystem::path& file, const Project* project) const
	{
		const auto base = std::filesystem::absolute(file).parent_path().lexically_normal();
		if (mEntities.size() > 100000) throw std::runtime_error("Project exceeds 100000 entities");
		const auto temporary = base / (Core::UUID::CreateNewUUID().str() + ".tmp");
		try
		{
			std::ofstream out(temporary, std::ios::binary);
			out.exceptions(std::ios::failbit | std::ios::badbit);
			out.imbue(std::locale::classic());
			out << std::setprecision(std::numeric_limits<float>::max_digits10);
			if (IsMap(file))
				out << "NuclearMap 2";
			else if (_wcsicmp(file.extension().c_str(), L".nuclearprefab") == 0)
				out << "NuclearProject 2\nproject " << std::quoted(ProjectName) << "\nassembly \"\"\nlevels 0";
			else
				throw std::runtime_error("Save a map with the .NMap extension");
			out << "\nprefabs " << Prefabs.size();
			for (const auto& [name, path] : Prefabs)
				out << "\nprefab " << std::quoted(name) << ' ' << std::quoted(Relative(path, base));
            out << "\nstartup " << std::quoted(SceneName);
			WriteVector(out, ClearColor);
			out << ' ' << mCamera << "\nentities " << mEntities.size() << '\n';
			for (size_t index = 0; index < mEntities.size(); ++index)
			{
				const auto& e = mEntities[index];
				out << "entity " << std::quoted(e.mName) << ' ' << std::quoted(e.mId) << ' ' << std::quoted(e.mParent) << "\ntransform";
				WriteVector(out, e.Transform.GetLocalPosition());
				const auto rotation = e.Transform.GetLocalRotation();
				WriteVector(out, Math::Vector4(rotation.x, rotation.y, rotation.z, rotation.w));
				WriteVector(out, e.Transform.GetLocalScale());
				out << "\nmesh " << bool(e.mMesh);
				if (e.mMesh)
				{
					const auto& m = *e.mMesh;
					out << ' ' << std::quoted(project ? project->ResourceReference(m.Mesh, base) : mReferences.at(index).first) << ' '
						<< std::quoted(project ? project->ResourceReference(m.Material, base) : mReferences.at(index).second) << ' ' << m.Visible
						<< ' ' << m.CastShadows << ' ' << m.ReceiveShadows;
                    WriteVector(out, m.Tint);
                    out << ' ' << m.Metallic << ' ' << m.Roughness << ' ' << m.ClipIndex << ' ' << m.BlendSeconds;
				}
				out << "\nlight " << bool(e.mLight);
				if (e.mLight)
				{
					const auto& l = *e.mLight;
					out << ' ' << int(l.Type);
					WriteVector(out, l.Direction);
					WriteVector(out, l.Color);
					WriteVector(out, l.Attenuation);
					WriteVector(out, l.Cone);
					out << ' ' << l.Intensity << ' ' << l.NearPlane << ' ' << l.FarPlane << ' ' << l.Fov << ' ' << l.CastShadows;
				}
				out << "\ncomponents " << e.mCamera << ' ' << e.mCollider << ' '
					<< (e.mKinematic ? int(*e.mKinematic) : -1) << ' ' << (e.mSkybox ? int(*e.mSkybox) : -1)
					<< ' ' << e.mListener << ' ' << e.mSource << ' ' << std::quoted(Relative(e.mClipPath, base))
					<< ' ' << e.mScript << ' ' << std::quoted(e.mScriptClass) << ' ' << e.mTrigger << ' ' << std::quoted(e.mFields) << '\n';
			}
			out << "end\n";
			if (out.tellp() > 32 * 1024 * 1024) throw std::runtime_error("Project exceeds 32 MiB");
			out.close();
			std::filesystem::rename(temporary, file);
		}
		catch (...)
		{
			std::error_code error;
			std::filesystem::remove(temporary, error);
			throw;
		}
	}
	ProjectDocument ProjectDocument::Read(const std::filesystem::path& file)
	{
		if (std::filesystem::file_size(file) > 32 * 1024 * 1024) throw std::runtime_error("Project exceeds 32 MiB");
		std::ifstream in(file, std::ios::binary);
		in.imbue(std::locale::classic());
		ProjectDocument doc;
		doc.mDirectory = std::filesystem::absolute(file).parent_path();
		const bool map = IsMap(file);
		Expect(in, map ? "NuclearMap" : "NuclearProject");
		const int version = Number(in, 1, map ? 2 : 3);
		if (!map)
		{
			Expect(in, "project");
			doc.ProjectName = String(in);
			Expect(in, "assembly");
			auto assembly = String(in);
			if (!assembly.empty()) doc.Assembly = (doc.mDirectory / assembly).lexically_normal();
		}
		if (!map && version >= 2)
		{
            Expect(in, "levels");
            const int levels = Number(in, 0, 1024);
            for (int index = 0; index < levels; ++index)
            {
                Expect(in, "level");
                auto name = String(in);
                auto path = String(in);
                if (name.empty() || path.empty() || (version == 3 && !IsMap(path)) || !doc.Levels.emplace(name, (doc.mDirectory / path).lexically_normal()).second)
                    throw std::runtime_error("Invalid or duplicate level name");
            }
        }
        if (version >= 2)
        {
            Expect(in, "prefabs");
            int count = Number(in, 0, 4096);
            for (int i = 0; i < count; ++i)
            {
                Expect(in, "prefab");
                auto name = String(in);
                auto path = String(in);
                if (name.empty() || path.empty() || !doc.Prefabs.emplace(name, (doc.mDirectory / path).lexically_normal()).second)
                    throw std::runtime_error("Empty or duplicate prefab entry");
            }
        }
		if (version == 3)
		{
			if (!IsProject(file)) throw std::runtime_error("Expected a main .nuclearproject file");
			Expect(in, "map");
			auto path = String(in);
			if (!IsMap(path)) throw std::runtime_error("Startup map must use .NMap");
			Expect(in, "end");
			if (!(in >> std::ws).eof()) throw std::runtime_error("Unexpected trailing project data");
			auto startup = (doc.mDirectory / path).lexically_normal();
			auto scene = Read(startup);
			scene.StartupMap = startup;
			scene.ProjectName = std::move(doc.ProjectName);
			scene.Assembly = std::move(doc.Assembly);
			scene.Levels = std::move(doc.Levels);
			scene.StartupPrefabs = std::move(scene.Prefabs);
			scene.Prefabs = std::move(doc.Prefabs);
			return scene;
		}
		Expect(in, "startup");
		doc.SceneName = String(in);
		doc.ClearColor = Vector<Math::Vector4>(in);
		doc.mCamera = Number(in, -1, 100000);
		Expect(in, "entities");
		const auto count = Number(in, 0, 100000);
		for (int index = 0; index < count; ++index)
		{
			auto& e = doc.mEntities.emplace_back();
			auto& refs = doc.mReferences.emplace_back();
			e.Exists = true;
			Expect(in, "entity");
			e.mName = String(in);
			if (version >= 2) { e.mId = String(in); e.mParent = String(in); }
			Expect(in, "transform");
			e.Transform.SetPosition(Vector<Math::Vector3>(in));
			auto q = Vector<Math::Vector4>(in);
			// Some existing authored transforms encode identity as four zeros.
			if (q != Math::Vector4(0.f) && std::abs(Math::dot(q, q) - 1.f) > .01f) throw std::runtime_error("Invalid entity rotation");
			e.Transform.SetRotation(Math::Quaternion(q.w, q.x, q.y, q.z));
			e.Transform.SetScale(Vector<Math::Vector3>(in));
			Expect(in, "mesh");
			if (Flag(in))
			{
				auto& m = e.mMesh.emplace();
				refs.first = String(in);
				refs.second = String(in);
				m.Visible = Flag(in);
				m.CastShadows = Flag(in);
				m.ReceiveShadows = Flag(in);
				if (version >= 2) { m.Tint = Vector<Math::Vector3>(in); m.Metallic = Number(in, -1.f, 1.f); m.Roughness = Number(in, -1.f, 1.f); m.ClipIndex = Number(in, -1, 65535); m.BlendSeconds = Number(in, 0.f, 3600.f); }
			}
			Expect(in, "light");
			if (Flag(in))
			{
				auto& l = e.mLight.emplace();
				l.Type = static_cast<Components::LightComponent::Type>(Number(in, 1, 3));
				l.Direction = Vector<Math::Vector3>(in);
				l.Color = Vector<Math::Vector3>(in);
				l.Attenuation = Vector<Math::Vector3>(in);
				l.Cone = Vector<Math::Vector2>(in);
				l.Intensity = Scalar(in);
				l.NearPlane = Scalar(in);
				l.FarPlane = Scalar(in);
				l.Fov = Scalar(in);
				l.CastShadows = Flag(in);
			}
			Expect(in, "components");
			e.mCamera = Flag(in);
			e.mCollider = Flag(in);
			int body = Number(in, -1, 1), sky = Number(in, -1, 1);
			if (body >= 0) e.mKinematic = body != 0;
			if (sky >= 0) e.mSkybox = sky != 0;
			e.mListener = Flag(in);
			e.mSource = Flag(in);
			auto clip = String(in);
			if (!clip.empty()) e.mClipPath = (doc.mDirectory / clip).lexically_normal().string();
			e.mScript = Flag(in);
			e.mScriptClass = String(in);
			if (version >= 2) { e.mTrigger = Flag(in); e.mFields = String(in); }
		}
		Expect(in, "end");
		if (!(in >> std::ws).eof()) throw std::runtime_error("Unexpected trailing project data");
		if (doc.mCamera >= count || (doc.mCamera >= 0 && !doc.mEntities[doc.mCamera].mCamera))
			throw std::runtime_error("Invalid game camera");
        std::unordered_map<std::string, std::string> parents;
        for (const auto& entity : doc.mEntities)
            if (!entity.mId.empty() && !parents.emplace(entity.mId, entity.mParent).second)
                throw std::runtime_error("Duplicate persistent entity ID");
        std::set<std::string> checked;
        for (const auto& [id, parent] : parents)
        {
            std::set<std::string> path;
            for (auto node = id; !node.empty() && !checked.contains(node); node = parents.at(node))
                if (!parents.contains(node) || !path.insert(node).second)
                    throw std::runtime_error("Invalid parent reference or cycle");
            checked.insert(path.begin(), path.end());
        }
        return doc;
    }
    void ProjectDocument::Resolve(Project& project)
	{
		std::map<std::filesystem::path, Assets::Mesh*> imports;
		for (size_t i = 0; i < mEntities.size(); ++i)
		{
			auto& m = mEntities[i].mMesh;
			if (!m) continue;
			m->Mesh = static_cast<Assets::Mesh*>(project.ResolveResource(mReferences.at(i).first, false, mDirectory, imports));
			m->Material = static_cast<Assets::Material*>(project.ResolveResource(mReferences.at(i).second, true, mDirectory, imports));
		}
	}
	entt::entity ProjectDocument::Instantiate() const
	{
		entt::entity camera = entt::null;
		for (size_t i = 0; i < mEntities.size(); ++i)
		{
			const auto& state = mEntities[i];
			auto entity = state.CreateCopy(state.GetName());
			if (!state.mId.empty()) Core::Scene::Get().GetRegistry().get<Components::EntityInfoComponent>(entity).PersistentId = state.mId;
			if (static_cast<int>(i) == mCamera) camera = entity;
		}
		return camera;
	}
	bool ProjectDocument::NeedsPhysics() const
	{
		return std::any_of(mEntities.begin(), mEntities.end(), [](const auto& e) { return e.mCollider || e.mKinematic.has_value(); });
	}
	bool ProjectDocument::NeedsAudio() const
	{
		return std::any_of(mEntities.begin(), mEntities.end(), [](const auto& e) { return e.mListener || !e.mClipPath.empty(); });
	}
	bool ProjectDocument::NeedsScripts() const
	{
		return std::any_of(mEntities.begin(), mEntities.end(), [](const auto& e) { return e.mScript && !e.mScriptClass.empty(); });
	}
}
