#include <AboutDocument.h>
#include <EditorWidgets.h>
#include <string>

extern "C" __declspec(dllimport) void* __stdcall ShellExecuteA(
	void* hwnd, const char* lpOperation, const char* lpFile,
	const char* lpParameters, const char* lpDirectory, int nShowCmd);
#pragma comment(lib, "Shell32.lib")

namespace Nuclear::Editor
{
	std::string AboutDocument::GetTitle()
	{
		return "About Nuclear Engine";
	}

	std::unique_ptr<UI::Widget> AboutDocument::Build()
	{
		auto scroll = std::make_unique<UI::ScrollBox>();
		auto& root = scroll->Add<UI::VerticalBox>();
		root.SetPadding({24, 20, 24, 20});
		root.SetSpacing(16);

		// Header
		auto& headerBox = root.Add<UI::VerticalBox>();
		headerBox.SetSpacing(4);
		auto& title = Label(headerBox, "NUCLEAR ENGINE", 24);
		title.SetColor({.18f, .7f, .62f, 1});
		Label(headerBox, "Modern C++20 3D Game Engine & Retained Native Editor", 13);

		// Section: Engine Build Information
		auto& engineSection = Section(root, "ENGINE BUILD");
		Label(engineSection, "Nuclear Engine Runtime", 13);
		Label(engineSection, std::string("Build Configuration: ") +
#ifdef _DEBUG
			"Debug x64"
#else
			"Release x64"
#endif
			+ "  |  Compiled: " __DATE__ " " __TIME__, 12);
		Label(engineSection, "Compiler: MSVC (" + std::to_string(_MSC_VER) + ")  |  C++ Standard: C++20  |  Target OS: Windows x64", 12);
		Label(engineSection, "Subsystems: Diligent Rendering (D3D11 / D3D12 / Vulkan), EnTT ECS, "
			"Nuclear.Managed Scripting (.NET 10 / C#), NVIDIA PhysX 4.1 Physics, Nuclear.Platform, "
			"Audio Plugins (XAudio2 & OpenAL)", 12).SetWrap(true);

		// Section: Editor Build Information
		auto& editorSection = Section(root, "EDITOR BUILD");
		Label(editorSection, "Nuclear Editor (Native Retained UI)", 13);
		Label(editorSection, "UI Architecture: Nuclear::UI (Pure Native Retained Widget Framework, Zero ImGui)", 12);
		Label(editorSection, std::string("Build Configuration: ") +
#ifdef _DEBUG
			"Debug x64"
#else
			"Release x64"
#endif
			+ "  |  Compiled: " __DATE__ " " __TIME__, 12);
		Label(editorSection, "Features: Multi-document docking workspace with tear-off native windows, "
			"Scene hierarchy with filtering, Transform gizmos (Move/Rotate/Scale), "
			"Asset browser with Unreal-style thumbnail tiles, Inspector with live property editing & undo/redo, "
			"Integrated performance profiler & session diagnostics", 12).SetWrap(true);

		// Section: Source Repository
		auto& repoSection = Section(root, "SOURCE REPOSITORY");
		Label(repoSection, "Official GitHub Repository:", 12);
		auto& repoRow = repoSection.Add<UI::HorizontalBox>();
		repoRow.SetSpacing(8);
		const std::string repoUrl = "https://github.com/CyberHazeGames/Nuclear-Engine";
		auto& urlBox = repoRow.Add<UI::TextBox>(repoUrl);
		Fill(repoRow, urlBox);
		Action(repoRow, "Open in Browser", [repoUrl]() {
			ShellExecuteA(nullptr, "open", repoUrl.c_str(), nullptr, nullptr, 1);
		});

		// Section: Engine License
		auto& licenseSection = Section(root, "ENGINE LICENSE");
		Label(licenseSection, "Copyright (c) 2017 - 2026 Nuclear Project", 13);
		Label(licenseSection, "Permission is hereby granted, free of charge, to any person obtaining a copy "
			"of this software and associated documentation files \"Nuclear Project\", to deal in the Software "
			"without restriction, including without limitation the rights to use, copy, modify, merge, publish, "
			"distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software "
			"is furnished to do so, subject to the following conditions:", 12).SetWrap(true);
		Label(licenseSection, "- The above copyright notice and this permission notice shall be included in all "
			"copies or substantial portions of the Software.", 12).SetWrap(true);
		Label(licenseSection, "- You agree to place the following notice in the credits for any Product developed "
			"with any component of (the \"Nuclear Project\"):\n"
			"  \"[Product] uses the Nuclear Engine.\"\n"
			"  \"Nuclear Engine, Copyright 2017 - 2026, Zone Org. All rights reserved.\"", 12).SetWrap(true);
		Label(licenseSection, "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
			"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR "
			"PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY "
			"CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
			"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.", 11).SetWrap(true);

		// Section: Third-Party Attributions
		auto& thirdPartySection = Section(root, "THIRD-PARTY ATTRIBUTIONS");
		Label(thirdPartySection, "- EnTT: Copyright (c) 2017-2022 Michele Caini (MIT License)", 11);
		Label(thirdPartySection, "- DiligentCore / Diligent Engine: Apache License 2.0", 11);
		Label(thirdPartySection, "- Open Asset Import Library (Assimp): 3-Clause BSD License", 11);
		Label(thirdPartySection, "- NVIDIA PhysX 4.1: Copyright (c) 2019 NVIDIA Corporation (3-Clause BSD License)", 11);
		Label(thirdPartySection, "- Nuclear.Managed / Coral: Copyright (c) 2023 Coral Contributors (MIT License)", 11);
		Label(thirdPartySection, "- spdlog: Copyright (c) 2016 Gabi Melman (MIT License)", 11);
		Label(thirdPartySection, "- msdf-atlas-gen: Copyright (c) 2020 Viktor Chlumsky (MIT License)", 11);
		Label(thirdPartySection, "- GLM (OpenGL Mathematics): Copyright (c) 2005-2012 G-Truc Creation (MIT License)", 11);

		return scroll;
	}
} // namespace Nuclear::Editor
