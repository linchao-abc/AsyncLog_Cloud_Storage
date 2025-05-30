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
                        { // 这个大括号起到划分作用域的功能， 除了作用域，锁自动释放
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
        using return_type = typename std::result_of<F(Args...)>::type; // 自动推理返回值类型
        //创建一个任务
        //std:bind 将f与args绑定成一个无参数调用，并用其构造一个packaged_task,返回指向packaged_task的智能指针
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future(); // 函数调用的返回值存在res
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if(stop) throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task]()
                            { (*task)();}); //发生隐式转换，使用lambda初始化了一个std::function<void()>对象
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
            worker.join(); // 阻塞等待每一个线程运行结束，并回收线程资源
        }
    }


private:
    std::vector<std::thread> workers;        // 线程队列
    std::queue<std::function<void()>> tasks; // 任务队列
    std::mutex queue_mutex;                  // 任务队列的互斥锁  
    std::condition_variable condition;       // 条件变量，用于任务队列的同步
    bool stop;                               // 控制线程池开启与关闭
};
