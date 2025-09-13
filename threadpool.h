#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex> 
#include<condition_variable> 
#include<functional>
#include<thread> 

// 线程pool 支持的模式 
enum class PoolMode{
    MODE_FIXED,
    MODE_CACHE, 
};

//
class Task{
public:
    virtual void run()=0; 


};
class Thread{
public:
    using ThreadFunc = std::function<void()>;
    Thread(ThreadFunc func); 
    ~Thread();
    void start();
private: 
    // std::thread th_; 
    ThreadFunc func_; 
};


class ThreadPool{
public:
    ThreadPool(); 
    ~ThreadPool();


    // 开启线程池 
    void start(int initThreadSize = 4); 
    // 设置工作线程
    void setMode(PoolMode mode); 

    // 设置任务队列大小
    void setTaskQueMaxThreshHold(int threshold);
    // 提交任务
    void submitTask(std::shared_ptr<Task> sp);

    //禁止拷贝赋值！ 
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool & operator= (const ThreadPool&) =delete ;
private: 
    std::vector<std::unique_ptr<Thread>> threads_; 
    size_t initThreadSize_; 
    // 裸指针 存在风险，当用户创建的任务对象是临时变量(非常容易非法访问)
    std::queue<std::shared_ptr<Task>> taskQue_; 
    std::atomic_int taskSize_;    //当前任务数量
    int taskQueMaxThreshHold_ ;    // 可处理任务数量上限
    // 线程互斥
    std::mutex taskQueMtx_; 
    std::condition_variable notFull_; 
    std::condition_variable notEmpty_; 
    // 前置下滑线 C++ 标准库的变量命名方式 

    PoolMode poolMode_;  
    //------定义线程函数------------
    // 为什么线程的回调函数在ThreadPool 创建， 一个是任务队列在线程池中，锁与条件变量 
    void threadFunc(); 

};


#endif 