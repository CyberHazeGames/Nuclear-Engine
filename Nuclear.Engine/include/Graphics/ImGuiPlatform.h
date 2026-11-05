#pragma once

#include <Nuclear.Platform/Event.h>
#include <Nuclear.Platform/Window.h>

namespace Nuclear::Graphics::ImGuiPlatform
{
	/// Attaches ImGui to a borrowed window; an ImGui context must already exist.
	bool Initialize(Platform::Window& window);
	/// Detaches the platform backend before its window or ImGui context is destroyed.
	void Shutdown();
	/// Updates display size, timing, and cursor before ImGui::NewFrame().
	void NewFrame();
	/// Submits one translated platform event to the active ImGui context, if any.
	void ProcessEvent(const Platform::Event& event);
}
