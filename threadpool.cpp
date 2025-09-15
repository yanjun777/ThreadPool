

# include "threadpool.h"
#include<functional>
#include<iostream>

const int TASK_MAX_THRESHHOLD= 1024 ;
const int THREAD_SIZE_THRESHHOLD = 10; 

ThreadPool::ThreadPool()
    : initThreadSize_(0)
    ,idleThreadSize_(0)
    ,curThreadSize_(0)
    ,threadSizeThreshHold_(THREAD_SIZE_THRESHHOLD)
    ,taskSize_(0)
    ,taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
    ,poolMode_(PoolMode::MODE_FIXED) 
    ,isPoolRunning_(false)
    
{ }

// 析构函数 如何释放线程资源？ 
ThreadPool::~ThreadPool(){}


// 设置工作线程
void ThreadPool::setMode(PoolMode mode){
    if(checkRunningState()){
        return;
    }
    poolMode_ = mode; 
}

// 设置线程池最大线程数量 CACHED 模式下有效
void ThreadPool::setThreadSizeThreshHold(int threshold){
    if(checkRunningState()){
        return;
    }
    if(poolMode_ == PoolMode::MODE_CACHED){
        threadSizeThreshHold_ = threshold;
    }
}

// 设置任务队列大小
void ThreadPool::setTaskQueMaxThreshHold(int threshold){
    if(checkRunningState()){
        return;
    }
    taskQueMaxThreshHold_ = threshold; 
}


// 开启线程池 
void ThreadPool::start(int initThreadSize){
    initThreadSize_ = initThreadSize; 
    curThreadSize_.store(initThreadSize);
    isPoolRunning_ = true;  // 线程池开始运行  必须放在线程开启之前 否则线程全部执行完毕然后销毁
    for(int i=0;i<initThreadSize_;i++){
        // 线程对象也是封装的！创建对象时，把线程函数给对象
        // Thread 构造函数必须接受一个参数 function<void()> func 
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
        int threadId = ptr->getThreadId();
        threads_.emplace(threadId,std::move(ptr));
        std::cout<<"init thread pool"<<"threadId:"<<threadId<<std::endl;
    }

    for(int i=0;i<initThreadSize_;i++){
        threads_[i]->start();  // 需要执行一个线程函数 
        idleThreadSize_.fetch_add(1);
    }
    
}
// 线程函数
void ThreadPool::threadFunc(int threadId){
    // std::cout<<"begin threadFunc tid"<<std::this_thread::get_id()
    //     <<std::endl;

    // std::cout<<"end threadFunc tid"<<std::this_thread::get_id()
    //     <<std::endl;
    auto lastTime = std::chrono::high_resolution_clock::now(); 
    for(;;){
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        // std::cout<<"tid:"<<std::this_thread::get_id()<<" wait task"<<std::endl;
        std::cout<<"threadId:"<<threadId<<" wait task"<<std::endl;
        if(poolMode_ == PoolMode::MODE_CACHED){
            // 有点不理解这里为什么要taskQue需要>0才等待 因为后面是有清除闲置线程的 而队列>0通常是不闲置的 
            // 如此设计 怎么删除闲置线程？ 旧设计 while(taskQue_.size()> 0) 才等待 出错了！
            // 9.15 为什么!=0 不需要加cv.wait? 因为此时有任务并且上文已获取锁 
            // 设计taskQue读写均需要上锁  因此不会脏读 
            while(taskQue_.size() == 0){
                if(std::cv_status::timeout == notEmpty_.wait_for(lock,std::chrono::seconds(1))){
                    auto now = std::chrono::high_resolution_clock::now(); 
                    auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime); 
                    if(dur > std::chrono::seconds(10)
                        && curThreadSize_ > initThreadSize_){
                        // 如果闲置时间超过10秒，则删除闲置线程
                        // 1. 将线程从线程列表中删除 
                        // 2. ThreadPool 线程数量对应变量及时更新 
                        // 3. 回收线程资源 return 就回收了！ 
                        threads_.erase(threadId);
                        curThreadSize_.fetch_sub(1);
                        idleThreadSize_.fetch_sub(1);
                        std::cout<<"threadId:"<<threadId<<" exit"<<std::endl; 
                        // std::cout<<"std threadId:"<<std::this_thread::get_id()<<" exit"<<std::endl; 
                        // std::cout<<"curThreadSize:"<<curThreadSize_<<std::endl; 
                        // std::cout<<"idleThreadSize:"<<idleThreadSize_<<std::endl; 
                        // break; //还不够 还没有完全删除线程
                        return; 
                    }
                }
            }
        }else{
            // 固定线程池模式 应该设置为超时模式，方便线程池析构销毁资源 
            if(!notEmpty_.wait_for(lock,std::chrono::seconds(100),[this]{
                return taskQue_.size() > 0;
            })){
                std::cout<<"taskQue is empty wait_for timeout "<<std::endl;
                continue;
            }
        }
        
        // std::cout<<"tid:"<<std::this_thread::get_id()<<" got task"<<std::endl;
        std::cout<<"threadId:"<<threadId<<" got task"<<std::endl;
        // 线程空闲数量减1
        idleThreadSize_.fetch_sub(1);
        // 取任务
        std::shared_ptr<Task> task = taskQue_.front();
        taskQue_.pop();
        taskSize_.fetch_sub(1);
        // 如果依然有任务，则继续通知消费者消费
        // 感觉没啥必要再通知一下
        if(taskSize_ > 0){
            notEmpty_.notify_all(); // 
        }
        // 通知生产者可以继续生产
        notFull_.notify_all();

        // 释放锁
        lock.unlock();
        // 执行任务
        if(task){
            task->exec();
        }
        // 线程空闲数量加1
        idleThreadSize_.fetch_add(1); 
    }
}
// 提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp){
    std::unique_lock<std::mutex> lock(taskQueMtx_); 
    if(!notFull_.wait_for(lock,std::chrono::seconds(1),[this]{
        return taskQue_.size() < taskQueMaxThreshHold_;
    })){
        std::cout<<"taskQue is full wait_for timeout "<<std::endl;
        return Result(sp,false);
    }
    taskQue_.emplace(sp);
    taskSize_.fetch_add(1);
    notEmpty_.notify_all(); 

    //需要根据线程池的运行状态来判断是否拓展线程池
    if(poolMode_ == PoolMode::MODE_CACHED 
        && taskSize_ > idleThreadSize_ 
        && curThreadSize_ < threadSizeThreshHold_){
            //这里使用curThreadSize_而不是threads_.size()是因为threads_.size()线程不安全
        // std::cout<<"thread pool is not full, need to add thread"<<std::endl;
        
        // 创建线程
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
        int threadId = ptr->getThreadId();
        threads_.emplace(threadId,std::move(ptr));
        // 启动线程
        threads_[threadId]->start();
        // 线程空闲数量加1  
        idleThreadSize_.fetch_add(1);
        // 当前线程池大小加1
        curThreadSize_.fetch_add(1);
        std::cout<<"extend thread pool"<<"threadId:"<<threadId<<std::endl;
    }
    return Result(sp);
}

bool ThreadPool::checkRunningState() const{
    return isPoolRunning_;
}
//---------线程方法实现--------------
int Thread::generateId_ = 0; 
Thread::Thread(ThreadFunc func){
    func_ =func;
    threadId_ = generateId_++; 
} 
Thread::~Thread(){

}
void Thread::start(){

    std::thread t(func_,threadId_); // C++11 线程对象t  和线程函数func_
    t.detach();  //分离线程 ！ 

}
int Thread::getThreadId() const{
    return threadId_;
} 

// ------------Task 实现--------------    
void Task::exec(){
    // 执行任务 
    if(result_ != nullptr){
        result_->setValue(run()); 
    }
}
Task::Task(){
    result_ = nullptr;
}
void Task::setResult(Result* result){
    result_ = result;
} 

// ------------Result 实现--------------    
Result::Result(std::shared_ptr<Task> task,bool isValid):task_(task),isValid_(isValid){
    task_->setResult(this); 
}
Any Result::get(){
    if(!isValid_){
        // 是针对线程池的提交任务的成功与否的判断 
        return "";
    }
    // 等待任务执行完成，返回结果 
    // 任务如果没有执行完，会阻塞在这里，等待任务执行完成 
    // 如果任务执行完了，会从任务中获取结果  count_用于条件判断无需唤醒 
    sem_.wait();
    return std::move(any_);
}

void Result::setValue(Any any){
    // 设置结果值 子线程设置结果值
    this->any_ = std::move(any);
    sem_.post();
}

// Result::Result(Result&& other):any_(std::move(other.any_)),sem_(std::move(other.sem_)),task_(std::move(other.task_)){
//     other.isValid_ = false;
// }
// Result& Result::operator=(Result&& other){
//     if(this != &other){
//         any_ = std::move(other.any_);
//         sem_ = std::move(other.sem_);
//         task_ = std::move(other.task_);
//     }
//     return *this;
// }

// ------------Semaphore 实现--------------     
Semaphore::Semaphore(Semaphore&& other):count_(std::move(other.count_)){
    other.count_ = 0;
}
Semaphore& Semaphore::operator=(Semaphore&& other){
    if(this != &other){
        count_ = std::move(other.count_);
    }
    return *this;
}
