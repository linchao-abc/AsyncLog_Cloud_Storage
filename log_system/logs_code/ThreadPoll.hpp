#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <future>

class ThreadPool {
public:
    // 初始化线程池，启动指定数量的线程
    ThreadPool(size_t threads) : stop(false) {
        for(size_t i = 0; i < threads; i++) {
            workers.emplace_back(
                [this]
                {
                    while(1) {
                        std::function<void()> task;
                        { // 这个大括号起到划分作用域的功能
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            // 等待任务队列不为空或线程池停止
                            this->condition.wait(
                                lock, [this]{return this->stop || !this->tasks.empty();}
                            );
                            if(this->stop && this->tasks.empty()) return;
                            // 取出任务
                            task = std::move(this->tasks.front());
                            this->tasks.pop();

                        }
                        // 出作用域，自动释放锁
                        task();
                    }
                }
            );
        }
    } 
    // 该函数用于将一个新任务添加到任务队列中，并返回一个 std::future 对象，用于获取任务的执行结果。
    template <class F, class... Args>
    auto enqueue(F &&f, Args &&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using return_type = typename std::result_of<F(Args...)>::type;
        //创建一个任务
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if(stop) throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task]()
                            { (*task)();});
        }
        condition.notify_one();
        return res;
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
        {
            worker.join();
        }
    }


private:
    std::vector<std::thread> workers;        // 线程队列
    std::queue<std::function<void()>> tasks; // 任务队列
    std::mutex queue_mutex;                  // 任务队列的互斥锁  
    std::condition_variable condition;       // 条件变量，用于任务队列的同步
    bool stop;                               // 控制线程池开启与关闭
};
