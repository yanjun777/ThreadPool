

# include "threadpool.h"
#include<functional>
#include<iostream>

const int TASK_MAX_THRESHHOLD= 1024 ;

ThreadPool::ThreadPool()
    : initThreadSize_(0)
    ,taskSize_(0)
    ,taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
    ,poolMode_(PoolMode::MODE_FIXED) 
{ }


ThreadPool::~ThreadPool(){}


// 设置工作线程
void ThreadPool::setMode(PoolMode mode){
    poolMode_ = mode; 
}

// 设置任务队列大小
void ThreadPool::setTaskQueMaxThreshHold(int threshold){
    taskQueMaxThreshHold_ = threshold; 
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
    return Result(sp);
}


// 开启线程池 
void ThreadPool::start(int initThreadSize){
    initThreadSize_ = initThreadSize; 

    for(int i=0;i<initThreadSize_;i++){
        // 线程对象也是封装的！创建对象时，把线程函数给对象
        // Thread 构造函数必须接受一个参数 function<void()> func 
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this));
        threads_.emplace_back(std::move(ptr));
    }

    for(int i=0;i<initThreadSize_;i++){
        threads_[i]->start();  // 需要执行一个线程函数 
    }
}
// 线程函数
void ThreadPool::threadFunc(){
    // std::cout<<"begin threadFunc tid"<<std::this_thread::get_id()
    //     <<std::endl;

    // std::cout<<"end threadFunc tid"<<std::this_thread::get_id()
    //     <<std::endl;
    for(;;){
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        std::cout<<"tid:"<<std::this_thread::get_id()<<" wait task"<<std::endl;
        if(!notEmpty_.wait_for(lock,std::chrono::seconds(100),[this]{
            return taskQue_.size() > 0;
        })){
            std::cout<<"taskQue is empty wait_for timeout "<<std::endl;
            continue;
        }
        std::cout<<"tid:"<<std::this_thread::get_id()<<" got task"<<std::endl;
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
    }
}
 
//---------线程方法实现--------------
Thread::Thread(ThreadFunc func){
    func_ =func;
} 
Thread::~Thread(){

}
void Thread::start(){

    std::thread t(func_); // C++11 线程对象t  和线程函数func_
    t.detach();  //分离线程 ！ 

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
