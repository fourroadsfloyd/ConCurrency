#pragma once

#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>

template<class T>
class No_Wait_Notify_ThreadSafe_Queue {
public:
	No_Wait_Notify_ThreadSafe_Queue() = default;
	No_Wait_Notify_ThreadSafe_Queue(const No_Wait_Notify_ThreadSafe_Queue& other)
	{
		std::lock_guard<std::mutex> lock(other._mutex);	//避免拷贝other时数据被修改
		_queue = other._queue;
	}
	No_Wait_Notify_ThreadSafe_Queue& operator=(const No_Wait_Notify_ThreadSafe_Queue& other) = delete; //禁止赋值操作
	void push(T new_value)	//编译器会自动优化移动语义
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_queue.push(std::move(new_value));
	}
	std::shared_ptr<T> pop()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (_queue.empty()) return nullptr;	//外部调用pop函数后，需要对返回值进行判断
		std::shared_ptr<T> res(std::make_shared<T>(std::move(_queue.front())));
		_queue.pop();
		return res;
	}
	void pop(T& value)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (_queue.empty()) return;		//外部调用pop函数后，需要对传入的value进行判断
		value = std::move(_queue.front());
		_queue.pop();
	}
	bool empty() const
	{
		std::lock_guard<std::mutex> lock(_mutex);
		return _queue.empty();
	}

private:
	std::queue<T> _queue;
	mutable std::mutex _mutex;
};

template<class T>
class Wait_Notify_ThreadSafe_Queue {
public:
	Wait_Notify_ThreadSafe_Queue() = default;
	Wait_Notify_ThreadSafe_Queue(const Wait_Notify_ThreadSafe_Queue& other)
	{
		std::lock_guard<std::mutex> lock(other._mutex);	//避免拷贝other时数据被修改
		_queue = other._queue;
	}

	Wait_Notify_ThreadSafe_Queue& operator=(const Wait_Notify_ThreadSafe_Queue& other) = delete; //禁止赋值操作

	void push(T new_value)	//编译器会自动优化移动语义
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_queue.push(std::move(new_value));
		_cond.notify_one();
	}

	std::shared_ptr<T> pop()
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_cond.wait(lock, [this]() {		//等待条件变量，避免虚假唤醒
			return !_queue.empty(); 
		}); 
		std::shared_ptr<T> res(std::make_shared<T>(std::move(_queue.front())));
		_queue.pop();
		return res;
	}

	void pop(T& value)
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_cond.wait(lock, [this]() {		//等待条件变量，避免虚假唤醒
			return !_queue.empty(); 
		}); 
		value = std::move(_queue.front());
		_queue.pop();
	}

	bool empty() const
	{
		std::lock_guard<std::mutex> lock(_mutex);
		return _queue.empty();
	}

private:
	std::queue<T> _queue;
	mutable std::mutex _mutex;
	std::condition_variable _cond;
};