

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
void ThreadPool::submitTask(std::shared_ptr<Task> sp){


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
    std::cout<<"begin threadFunc tid"<<std::this_thread::get_id()
        <<std::endl;

    std::cout<<"end threadFunc tid"<<std::this_thread::get_id()
        <<std::endl;
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


