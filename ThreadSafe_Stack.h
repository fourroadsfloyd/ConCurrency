#pragma once

#include <stack>
#include <mutex>
#include <memory>
#include <condition_variable>

template<class T>
class No_Wait_Notify_ThreadSafe_Stack {
public:
	No_Wait_Notify_ThreadSafe_Stack() = default;
	No_Wait_Notify_ThreadSafe_Stack(const No_Wait_Notify_ThreadSafe_Stack& other)
	{
		std::lock_guard<std::mutex> lock(other._mutex);	//避免拷贝other时数据被修改
		_stack = other._stack;
	}

	No_Wait_Notify_ThreadSafe_Stack& operator=(const No_Wait_Notify_ThreadSafe_Stack& other) = delete; //禁止赋值操作

	void push(T new_value)	//编译器会自动优化移动语义
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_stack.push(std::move(new_value));
	}

	std::shared_ptr<T> pop()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (_stack.empty()) return nullptr;	//外部调用pop函数后，需要对返回值进行判断
		std::shared_ptr<T> res(std::make_shared<T>(std::move(_stack.top())));
		_stack.pop();
		return res;
	}

	void pop(T& value)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (_stack.empty()) return;		//外部调用pop函数后，需要对传入的value进行判断
		value = std::move(_stack.top());
		_stack.pop();
	}

	bool empty() const
	{
		std::lock_guard<std::mutex> lock(_mutex);
		return _stack.empty();
	}

private:
	std::stack<T> _stack;
	mutable std::mutex _mutex;
};


//生产者消费者模式下的线程安全的栈
template<class T>
class Wait_Notify_ThreadSafe_Stack {
public:
	Wait_Notify_ThreadSafe_Stack() = default;
	Wait_Notify_ThreadSafe_Stack(const Wait_Notify_ThreadSafe_Stack& other)
	{
		std::lock_guard<std::mutex> lock(other._mutex);	//避免拷贝other时数据被修改
		_stack = other._stack;
	}

	Wait_Notify_ThreadSafe_Stack& operator=(const Wait_Notify_ThreadSafe_Stack& other) = delete; //禁止赋值操作

	void push(T new_value)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_stack.push(std::move(new_value));
		_cond.notify_one();
	}

	std::shared_ptr<T> pop()
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_cond.wait(lock, [this]() {
			return !_stack.empty();
		});

		std::shared_ptr<T> res(std::make_shared<T>(std::move(_stack.top())));
		_stack.pop();
		
		return res;
	}

	void pop(T& value)
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_cond.wait(lock, [this]() {
			return !_stack.empty();
		});

		value = std::move(_stack.top());
		_stack.pop();
	}

	bool empty() const
	{
		std::lock_guard<std::mutex> lock(_mutex);
		return _stack.empty();
	}

private:
	std::stack<T> _stack;
	mutable std::mutex _mutex;
	std::condition_variable _cond;
};