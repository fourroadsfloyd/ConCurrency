#include <thread>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <atomic>

/*
    input: n
    output: 01020304...n
    中间穿插0

    Zero 0
    Odd 奇数
    Even 偶数
*/

class ZeroEvenOdd{
public:
    ZeroEvenOdd(int n) : n(n), printed(0), next(0), stop(false) {}

    void Zero() //每次打印0后，通知奇数或偶数线程打印数字，直到打印完n个数字后，设置stop标志位为true，通知所有线程结束。
    {
        for(int i=0;i<n;++i)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this](){
                return next == 0;
            });
            std::cout << 0;
            ++printed;
            next = printed % 2 == 0 ? 2 : 1;
            cv.notify_all();
        }

        stop.store(true, std::memory_order_release);
    }

    void Odd()
    {
        while(true)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this](){ //等待条件：要么是奇数线程的轮次，要么是stop标志位被设置为true，表示打印结束。
                return next == 1 || stop.load(std::memory_order_acquire);
            });

            if(next == 1 && stop.load(std::memory_order_acquire))   //如果是奇数线程的轮次，并且stop标志位被设置为true，表示打印结束，直接打印当前数字并退出循环。
            {
                std::cout << printed;
                break;
            }
                
            if(stop.load(std::memory_order_acquire))    //如果stop标志位被设置为true，表示打印结束，直接退出循环。
                break;
            
            std::cout << printed;
            next = 0;   //打印完数字后，切换回0线程的轮次，通知0线程继续打印。
            cv.notify_all();
        }
    }

    void Even()
    {
        while(true)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this](){
                return next == 2 || stop.load(std::memory_order_acquire);
            });

            if(next == 2 && stop.load(std::memory_order_acquire))
            {
                std::cout << printed;
                break;
            }

            if(stop.load(std::memory_order_acquire))
                break;

            std::cout << printed;
            next = 0;   //打印完数字后，切换回0线程的轮次，通知0线程继续打印。
            cv.notify_all();
        }
    }

private:
    int n;
    int printed;    //打印的数字
    std::atomic<bool> stop; // 标志位，表示是否继续打印
    std::mutex mtx;
    std::condition_variable cv;
    int next; // 0 for zero, 1 for odd, 2 for even
};

int main()
{
    int n_input;
    std::cin >> n_input;
    ZeroEvenOdd obj(n_input);

    std::thread t1(&ZeroEvenOdd::Zero, &obj);
    std::thread t2(&ZeroEvenOdd::Odd, &obj);
    std::thread t3(&ZeroEvenOdd::Even, &obj);

    t1.join();
    t2.join();
    t3.join();

    return 0;
}

/*
std::thread构造时，如果传入的是成员函数指针，需要同时传入对象的指针或引用，以便成员函数能够访问对象的成员变量和成员函数。
对于普通函数，直接传入函数指针或函数名称即可，无需对象指针或引用。
*/