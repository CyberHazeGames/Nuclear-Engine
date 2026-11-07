#pragma once

#include <Core/Client.h>

/// Minimal runtime UI client. All visible sample widgets use the dedicated UI renderer.
class UISample final : public Nuclear::Core::Client
{
public:
	/// Attaches the showcase composition and selects normal-cursor UI input.
	void Load() override;
	/// Clears the background before the engine composes retained UI.
	void Render(float deltaTime) override;
	/// Resizes the sample's swap chain to the physical client dimensions.
	void OnWindowResize(int width, int height) override;
};
