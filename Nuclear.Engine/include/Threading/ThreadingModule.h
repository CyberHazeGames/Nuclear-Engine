#pragma once
#include <Core/EngineModule.h>
#include <Threading/ThreadPool.h>
#include <Threading/MainThreadTask.h>

namespace Nuclear
{
    namespace Threading
    {
        class NEAPI ThreadingModule : public Core::EngineModule
        {
        public:
            static ThreadingModule& Get();
            bool OnInitialize() override;
            bool Initialize();

            void Shutdown() override;
            void AddTask(Task* task);
            void AddMainThreadTask(MainThreadTask* task);

            ThreadPool& GetThreadPool();
            void ExecuteMainThreadTasks(Uint32 count);
            std::vector<MainThreadTask*>& GetMainTasks();
        protected:
            std::thread::id mMainThreadID;
            ThreadPool mMainPool;
            std::vector<MainThreadTask*> mMainTTasks;
            std::mutex mMainTTasksMutex;
        private:
            ThreadingModule();
        };

    }
}
