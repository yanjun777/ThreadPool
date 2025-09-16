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
#include<unordered_map> 

// 线程pool 支持的模式 
enum class PoolMode{
    MODE_FIXED,
    MODE_CACHED, 
};

class Any{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any& other)=delete;
    Any& operator=(const Any& other)=delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    template<typename T>
    Any(T data):base_(std::make_unique<Derived<T>>(data)){}

    template<typename T>
    T cast_(){
        Derived<T>* ptr = dynamic_cast<Derived<T>*>(base_.get());
        if(ptr == nullptr){
            throw std::bad_cast();
        }
        return ptr->data_;
    }

private:
    // 基类 多态特性实现接受不同类型的数据 
    class Base{
    public:
        virtual ~Base() = default;
    };
    // 派生类 接受数据
    template<typename T>
    class Derived : public Base{
    public:
        Derived(T data):data_(data){}
        T data_;
    };

private:
    std::unique_ptr<Base> base_;
};

class Semaphore{
public:
    Semaphore(int count = 0):count_(count){}
    ~Semaphore() = default;
    Semaphore(const Semaphore& other)=delete;
    Semaphore& operator=(const Semaphore& other)=delete;
    Semaphore(Semaphore&& other);
    Semaphore& operator=(Semaphore&& other);
    void wait(){
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]{ return count_ > 0; });
        count_--;
    }

    void post(){
        std::unique_lock<std::mutex> lock(mtx_);
        count_++;
        cv_.notify_all();
    }
private:
    int count_;
    std::mutex mtx_;
    std::condition_variable cv_;
};
class Task;
// result 
class Result{
public:
    Result(std::shared_ptr<Task> task,bool isValid = true);
    ~Result() = default;
    Result(const Result& other)=delete;
    Result& operator=(const Result& other)=delete;
    // Result(Result&& other);
    // Result& operator=(Result&& other); 
    Any get();
    void setValue(Any any);
private:
    Any any_;
    Semaphore sem_; 
    std::shared_ptr<Task> task_; 
    std::atomic_bool isValid_; 
};

//
class Task{
public:
    Task();
    ~Task() = default;
    virtual Any run()=0; 
    // 封装执行任务函数，用于返回结果 
    void exec();
    void setResult(Result* result);
private:
    Result* result_;
};

class Thread{
public:
    using ThreadFunc = std::function<void(int)>;
    Thread(ThreadFunc func); 
    ~Thread();
    void start();
    int getThreadId() const;
private: 
    // std::thread th_; 
    static int generateId_;  // 线程id生成器
    ThreadFunc func_; 
    int threadId_; 
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
    // 设置线程池最大线程数量 CACHED 模式下有效
    void setThreadSizeThreshHold(int threshold);
    // 提交任务
    Result submitTask(std::shared_ptr<Task> sp);

    //禁止拷贝赋值！ 
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool & operator= (const ThreadPool&) =delete ;
private: 
    // 线程列表
    std::unordered_map<int,std::unique_ptr<Thread>> threads_; 
    // std::vector<std::unique_ptr<Thread>> threads_; 
    // 初始线程池大小
    size_t initThreadSize_; 
    // 最大线程池大小
    size_t threadSizeThreshHold_; 
    // 当前线程池大小
    std::atomic_size_t curThreadSize_; 
    std::atomic_size_t idleThreadSize_; 

    // 裸指针 存在风险，当用户创建的任务对象是临时变量(非常容易非法访问)
    std::queue<std::shared_ptr<Task>> taskQue_; 
    std::atomic_int taskSize_;    //当前任务数量
    int taskQueMaxThreshHold_ ;    // 可处理任务数量上限
    // 线程互斥
    std::mutex taskQueMtx_; 
    std::condition_variable notFull_; 
    std::condition_variable notEmpty_; 
    std::condition_variable exitCond_; 
    // 前置下滑线 C++ 标准库的变量命名方式 

    // 线程池模式 
    PoolMode poolMode_; 
    std::atomic_bool isPoolRunning_; 
   
    //------定义线程函数------------
    // 为什么线程的回调函数在ThreadPool 创建， 一个是任务队列在线程池中，锁与条件变量 
    void threadFunc(int threadId);  // 线程函数 
    bool checkRunningState() const; 

};


#endif 