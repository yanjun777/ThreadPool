

#include "threadpool.h"

#include<iostream>
#include<chrono>
#include<thread>
#include<vector>
using namespace std;

using uLong = unsigned long long ;
// 任务类 todo 修改为并行处理加法运行 
class MyTask : public Task{
    public:
    MyTask(int begin, int end):begin_(begin),end_(end){}
    Any run(){
        std::cout<<"tid "<<std::this_thread::get_id()<<" begin"<<std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uLong sum = 0;
        for(int i = begin_; i <= end_; i++){
            sum += i;
        }
        std::cout<<"tid "<<std::this_thread::get_id()<<" end"<<std::endl;
        return sum; 
    }
private:
    int begin_;
    int end_;
};  

int main(){

    ThreadPool pool ;
    pool.start(4); 

    Result res = pool.submitTask(std::make_shared<MyTask>(1, 1000000));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(1000001  , 2000000));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(2000001, 3000000));
    Result res4 = pool.submitTask(std::make_shared<MyTask>(3000001, 4000000));

    cout<<"doing task"<<endl;
    uLong sum = 0 ;
    // MyTask 的 run 方法返回的必須是 uLong 
    sum += res.get().cast_<uLong>();
    sum += res2.get().cast_<uLong>();
    sum += res3.get().cast_<uLong>();
    sum += res4.get().cast_<uLong>();

    cout<<"fuckfuckfuckressum: "<<sum<<endl;
    cout<<"finish task"<<endl;
    
#if 0
    // bug 代码 增加了 Semophore 和Result 的移动构造还是不行！ 
    vector<Result> res; 
    res.reserve(10);
    for(int i = 0; i < 10; i++){
        auto sp = std::make_shared<MyTask>(i, i+100);
        res.push_back(pool.submitTask(sp));  
    }
    long long sum = 0 ;
    for(int i = 0; i < res.size(); ++i){
        sum += res[i].get().cast_<long long >();
    }
    cout<<"fuckfuckfuckres: "<<sum<<endl;
#endif 
    getchar();

}
