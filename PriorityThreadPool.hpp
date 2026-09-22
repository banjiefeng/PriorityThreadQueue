# include <iostream>
# include <condition_variable>
# include <mutex>
# include <future>
# include <thread>
# include <vector>
# include <queue>
# include <functional>
# include <memory>

struct Task
{
    int priority;
    std::function<void()> f;

    bool operator<(const Task& other) const
    {
        return priority > other.priority;
    }
};

class PriorityThreadPool{
public:
    explicit PriorityThreadPool(std::size_t nums)
    {
        for(std::size_t i = 0; i < nums; i ++)
        workers_.emplace_back([this](){
            return worker_loop();});
    }

    void enqueue(int priority, std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            Task t = {priority, task};
            tasks_.push(std::move(t));
        }
        cv_.notify_one();
    }

    ~PriorityThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }

        cv_.notify_all();

        for(auto& it: workers_)
        it.join();
    }

    template<typename F, typename... Args>
    auto submit(int priority, F&& f, Args&&... args)
    //根据函数模板进行返回值推导
        ->std::future<std::invoke_result_t<F, Args...>>
    {
        //获取返回值的类型
        using ReturnType = std::invoke_result_t<F, Args...>;
        //声明packaged_task任务，并进行参数绑定
        auto task = std::make_shared<std::packaged_task<ReturnType()>>
            (std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        //绑定future
        std::future<ReturnType> fut = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mtx_);
            if(stop_)
            {
                throw std::runtime_error("线程池已停止，无法提交新任务");
            }

            tasks_.push({priority, [task](){
                (*task)();
            }});
        }

        cv_.notify_one();
        return fut;
    }

    // 禁止拷贝和移动
    PriorityThreadPool(const PriorityThreadPool&) = delete;
    PriorityThreadPool& operator=(const PriorityThreadPool&) = delete;
    PriorityThreadPool(PriorityThreadPool&&) = delete;
    PriorityThreadPool& operator=(PriorityThreadPool&&) = delete;

private:

    void worker_loop()
    {
        while(true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [this](){
                    return stop_ || !tasks_.empty();
                });

                if(stop_ && tasks_.empty())
                {
                    return ;
                }

                task = std::move(tasks_.top().f);
                tasks_.pop();
            }
            // 单独一个作用域是为了保证任务执行不会在锁内
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::priority_queue<struct Task> tasks_;
    bool stop_{false};
};

int compute(int x)
{
    return x * x;
}