#pragma once
#include <NE_Common.h>
namespace Nuclear::Core
{
	// Common lifecycle for owned modules and engine subsystem singletons.
	class NEAPI EngineModule
	{
	public:
		virtual ~EngineModule() = default;
		EngineModule(const EngineModule&) = delete;
		EngineModule& operator=(const EngineModule&) = delete;

		virtual bool OnLoad() { return true; }
		virtual bool OnInitialize() { return true; }
		virtual bool OnStart() { return true; }
		virtual void OnUpdate(float) {}
		virtual void OnStop() {}
		virtual void OnUnload() {}
		virtual void Shutdown() = 0;

	protected:
		EngineModule() = default;
	};
}
