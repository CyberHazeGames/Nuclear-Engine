#include "EditorUI.h"
#include "Nuclear.Editor.h"
#include <type_traits>
#include <Profiling/ProfilingModule.h>

#include <ECS/entt/core/type_info.hpp>
using namespace entt::literals;

namespace Nuclear::Editor 
{
	// Helper to display a little (?) mark which shows a tooltip when hovered.
	// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
	static void HelpMarker(const char* desc)
	{
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}
	EditorUI::EditorUI(NuclearEditor* editor)
		:mAssetLibViewer(editor)
	{
		mEditorInstance = editor;
		pActiveProject = nullptr;
	}
	void EditorUI::SetProject(Project* project)
	{
		mAssetLibViewer.SetProject(project);
		pActiveProject = project;
	}
	void EditorUI::NewProjectWindow()
	{
		if (ImGui::BeginPopupModal("New Project", &mNewPrjWindowOpen))
		{
			std::vector<std::string> ProjectTemplates;
			ProjectTemplates.push_back("Empty 2D project");
			ProjectTemplates.push_back("Empty 3D/2D project");

			static bool firstpage = true, secondpage = false;
			static int selected = 0;

			//Project Info
			static char ProjectName[128] = "NewProject";


			if (firstpage)
			{
				if (ImGui::Button("Create"))
				{
					pActiveProject = &mEditorInstance->mActiveProject;
					ProjectInfo info;
					info.mProjectName = ProjectName;
					pActiveProject->Initalize(info);

					mAssetLibViewer.SetProject(pActiveProject);
					pActiveProject->AddNewScene();

					//Finish close window
					mNewPrjWindowOpen = false;
					ImGui::CloseCurrentPopup();
				}
				// Left
				{
					ImGui::BeginChild("left pane", ImVec2(150, 0), true);

					ImGui::Text("Project Templates:");
					ImGui::Separator();
					for (int i = 0; i < ProjectTemplates.size(); i++)
					{
						if (ImGui::Selectable(ProjectTemplates.at(i).c_str(), selected == i))
						{
							selected = i;
						}
					}
					ImGui::EndChild();
				}
				ImGui::SameLine();

				// Right
				{
					ImGui::BeginGroup();
					ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us
					ImGui::Text(ProjectTemplates.at(selected).c_str());
					ImGui::Separator();
					if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None))
					{
						if (ImGui::BeginTabItem("Description"))
						{
							ImGui::TextWrapped("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ");
							ImGui::EndTabItem();
						}
						if (ImGui::BeginTabItem("Details"))
						{
							ImGui::Text("ID: 0123456789");
							ImGui::EndTabItem();
						}
						ImGui::EndTabBar();
					}
					ImGui::EndChild();
					if (ImGui::Button("Cancel")) { mNewPrjWindowOpen = false; ImGui::CloseCurrentPopup(); }
					ImGui::SameLine();
					if (ImGui::Button("Next")) { firstpage = false; secondpage = true; }
					ImGui::EndGroup();
				}
			}
			else if (secondpage)
			{
				ImGui::Text("Project Configuration");



				ImGui::Separator();




				if (ImGui::Button("Back")) { firstpage = true; secondpage = false; }
				ImGui::SameLine();
				if (ImGui::Button("Next"))
				{
					firstpage = false; secondpage = false;
				}
			}
			else
			{
				ImGui::Text("Project Name");
				ImGui::InputText("##newprjlbl", ProjectName, IM_ARRAYSIZE(ProjectName));
				ImGui::Separator();

				if (ImGui::Button("Back")) { firstpage = false; secondpage = true;  }
				ImGui::SameLine();
				if (ImGui::Button("Create"))
				{
					pActiveProject = &mEditorInstance->mActiveProject;
					ProjectInfo info;
					info.mProjectName = ProjectName;
					pActiveProject->Initalize(info);

					mAssetLibViewer.SetProject(pActiveProject);
					pActiveProject->AddNewScene();

					//Finish close window
					mNewPrjWindowOpen = false;
					ImGui::CloseCurrentPopup();
				}

				//LOCATION MISSING
			}

			ImGui::EndPopup();
		}

		//if (!pActiveProject)
		//{
		//	pActiveProject = &mEditorInstance->mActiveProject;
		//}
	}
	void EditorUI::Render()
	{
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			constexpr ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
			constexpr ImGuiWindowFlags   window_flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

			ImGui::Begin("MainDockSpace", nullptr, window_flags);
			ImGui::PopStyleVar(3);
			ImGuiID dockspace_id = ImGui::GetID("DockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			ImGui::End();
		}

		RenderMainMenuBar();
		RenderStatusBar();
		if (mProfilerWindowOpen)
		{
			Profiling::DrawProfilerWindow(&mProfilerWindowOpen);
		}
		if (mNewPrjWindowOpen)
		{
			ImGui::OpenPopup("New Project");
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			NewProjectWindow();
		}

		if (pActiveProject)
		{
			pActiveProject->ShowProjectFolderView();
			RenderEntityExplorer();
			mAssetLibViewer.Render();
		}
		else {
			//no project
		}
	}

	void EditorUI::RenderSceneView()
	{
		ImGui::Begin("Scene View");

		ImGui::End();
	}

	void EditorUI::RenderStatusBar()
	{
		auto viewport = ImGui::GetMainViewport();


		// Set position to the bottom of the viewport
		ImGui::SetNextWindowPos(
			ImVec2(viewport->Pos.x,
				viewport->Pos.y + viewport->Size.y - ImGui::GetFrameHeight()));

		// Extend width to viewport width
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, ImGui::GetFrameHeight()));

		// Add menu bar flag and disable everything else
		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_MenuBar;

		if (ImGui::Begin("StatusBar", nullptr, flags)) {
			if (ImGui::BeginMenuBar()) {
				auto adapter = Graphics::GraphicsModule::Get().GetAdapterInfo();
				ImGui::Text("Nuclear Engine 0.01A Running On: %s", adapter.Description);
				ImGui::EndMenuBar();
			}
			ImGui::End();
		}
	}

	void EditorUI::RenderMainMenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			//Scene / Project
			if (ImGui::BeginMenu("Window"))
			{
				ImGui::MenuItem("Performance & Memory Profiler", nullptr, &mProfilerWindowOpen);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::BeginMenu("New"))
				{
					if (ImGui::MenuItem("Project"))
					{
						//TODO: Multiple Projects
						mNewPrjWindowOpen = true;
						//ImGui::OpenPopup("NewProject");

					}
					ImGui::Separator();
					if (ImGui::MenuItem("Scene", NULL, false, (pActiveProject == nullptr) ? false : true))
					{
						pActiveProject->AddNewScene();
					}

					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
				if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
				ImGui::Separator();
				if (ImGui::MenuItem("Cut", "CTRL+X")) {}
				if (ImGui::MenuItem("Copy", "CTRL+C")) {}
				if (ImGui::MenuItem("Paste", "CTRL+V")) {}
				ImGui::EndMenu();
			}

			bool addmenuacive = false;
			if (pActiveProject)
			{
				if (Core::Scene::Get().GetSceneAsset())
				{
					addmenuacive = true;
				}
			}
			if (ImGui::BeginMenu("Add", addmenuacive))
			{
				if (ImGui::MenuItem("Empty Entity"))
				{
					 Core::Scene::Get().CreateEntity();
				}
				if (ImGui::MenuItem("Box")) 
				{
					 Core::Scene::Get().CreateBox(pActiveProject->GetDefaultMaterial());
				}

				if (ImGui::MenuItem("Sphere"))
				{
					 Core::Scene::Get().CreateSphere(pActiveProject->GetDefaultMaterial());
				}

				if (ImGui::MenuItem("Plane"))
				{
					 Core::Scene::Get().CreatePlane(pActiveProject->GetDefaultMaterial());
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Systems", addmenuacive))
			{
				if (ImGui::MenuItem("Render System"))
				{
					Core::Scene::Get().CreateEntity();
				}

				ImGui::EndMenu();
			}


			ImGui::EndMainMenuBar();
		}
	}

	void EditorUI::RenderEntityExplorer()
	{
		ImGui::Begin("Entity Explorer");

		if (ImGui::TreeNode(Core::Scene::Get().GetName().c_str()))
		{
			auto view = Core::Scene::Get().GetRegistry().view<Components::EntityInfoComponent>();

			for (auto entity : view)
			{
				auto& Einfo = view.get<Components::EntityInfoComponent>(entity);
				auto index = (Uint32)entity;

				ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

				static int selectedindex = 0;

				if (selectedindex == index)
				{
					node_flags |= ImGuiTreeNodeFlags_Selected;
					mEditor.Render(entity, Core::Scene::Get().GetRegistry(), Einfo);
				}

				ImGui::PushID(index);


				ImGui::TreeNodeEx(Einfo.mName.c_str(), node_flags);
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				{
					selectedindex = index;
				}

				ImGui::PopID();
			}
			ImGui::TreePop();
		}

		if (ImGui::Button("Add Entity"))
		{
			Core::Scene::Get().CreateEntity();
		}
		ImGui::End();
	}
	

}
