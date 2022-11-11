#include "TaskGenerate.h"

using DatasType = std::shared_ptr<std::map<std::string, std::shared_ptr<TensorValue<float>>>>;

void AddRequestInThread(std::mutex *mutex, std::condition_variable *condition, int index, ExecutorManager *executorManager, std::vector<std::pair<std::string, ModelInputCreator>> *inputCreators, int count, float lambda, int *current_count)
{
    static std::atomic<int> flag=0;
    PossionRandom possionRandom(index * 100+100);
    UniformRandom uniformRandom(index * 10+10);
    std::pair<std::string, ModelInputCreator> *creator = &(*inputCreators)[index];
    std::vector<std::pair<std::string, DatasType>> datas(20);
    for (int i = 0; i < 20; i++)
    {
        datas[i] = std::pair<std::string, DatasType>(creator->first, creator->second.CreateInput());
    }

    flag++;

    while(flag<inputCreators->size())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout<<"start to send request for "<<creator->first<<"."<<std::endl;

    int i = 0;
    int tmp=0;
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds((int)possionRandom.Random(lambda)));
        std::unique_lock<std::mutex> lock(*mutex);

        if (*current_count >= count)
        {
            // to be end.
            break;
        }

        tmp = *current_count;
        (*current_count)++;
        lock.unlock();
        condition->notify_all();
            executorManager->AddTask(datas[i%20].first, datas[i%20].second, std::to_string(tmp+1));
        i++;
    }
    std::cout<<"end send "<<creator->first<<", total: "<<i<<std::endl;
}

void ReqestGenerate(ExecutorManager *executorManager, std::vector<std::pair<std::string, ModelInputCreator>> *inputCreators, int count, float lambda)
{
    std::cout << "wait system to init..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "run request generate with possion, and start to prepare random input." << std::endl;
    
    std::condition_variable condition;
    std::vector<std::shared_ptr<std::thread>> threads;
    int current_count=0;
    std::mutex mutex;
    for(int i=0;i<inputCreators->size();i++)
    {
        threads.push_back(std::make_shared<std::thread>(AddRequestInThread,&mutex,&condition,i,executorManager,inputCreators,count,lambda,&current_count));
    }

    for(auto thread: threads)
    {
        thread->join();
    }

    std::cout << "end send request." << std::endl;
}

void ReplyGather(ExecutorManager *executorManager, int count)
{
    std::cout << "run reply gather." << std::endl;
#ifdef PARALLER_MODE
    std::filesystem::path saveFold = RootPathManager::GetRunRootFold() / "data" / "PARALLEL";
#elif OYST_MODE
    std::filesystem::path saveFold = RootPathManager::GetRunRootFold() / "data" / "OYST";
#elif BNST_MODE
    std::filesystem::path saveFold = RootPathManager::GetRunRootFold() / "data" / "BNST";
#elif FIFO_MODE
    std::filesystem::path saveFold = RootPathManager::GetRunRootFold() / "data" / "FIFO";
#else
    std::filesystem::path saveFold = RootPathManager::GetRunRootFold() / "data" / "DLIR";
#endif

    std::filesystem::remove_all(saveFold);
    std::filesystem::create_directories(saveFold);
    auto &applyQueue = executorManager->GetApplyQueue();
    for (int i = 0; i < count; i++)
    {
        auto task = applyQueue.Pop();
        JsonSerializer::StoreJson(task->GetDescribe(), saveFold / (std::to_string(i) + ".json"));
    }
    std::cout << "all apply for " << count << " task received." << std::endl;
}
