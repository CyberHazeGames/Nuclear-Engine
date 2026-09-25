#include "Threading/ThreadingModule.h"
#include <Utilities/Logger.h>
namespace Nuclear
{
    namespace Threading
    {
        ThreadingModule& ThreadingModule::Get() { static ThreadingModule instance; return instance; }
        bool ThreadingModule::OnInitialize() { return Initialize(); }
        ThreadingModule::ThreadingModule()
        {

        }

        bool ThreadingModule::Initialize()
        {
            mMainThreadID = std::this_thread::get_id();

            //TODO: Clamp
            auto allowedthreads = std::thread::hardware_concurrency() - 1;
            mMainPool.Initialize(allowedthreads);

            NUCLEAR_INFO("[ThreadingModule] ThreadingModule has been initialized succesfully!");
            return true;
        }
        void ThreadingModule::Shutdown()
        {
            mMainPool.Shutdown();
        }
        void ThreadingModule::AddTask(Task* task)
        {
            mMainPool.AddTask(task);
        }
        void ThreadingModule::AddMainThreadTask(MainThreadTask* task)
        {
            mMainTTasksMutex.lock();
            mMainTTasks.push_back(task);
            mMainTTasksMutex.unlock();
        }
        ThreadPool& ThreadingModule::GetThreadPool()
        {
            return mMainPool;
        }
        void ThreadingModule::ExecuteMainThreadTasks(Uint32 count)
        {
            for (Uint32 i = 0; i < mMainTTasks.size() && i < count; i++)
            {
                mMainTTasks.at(i)->Execute();
                mMainTTasks.erase(mMainTTasks.begin() + i);
            }
         
        }
        std::vector<MainThreadTask*>& ThreadingModule::GetMainTasks()
        {
            return mMainTTasks;
        }
    }
}
