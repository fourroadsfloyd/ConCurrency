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

	void push(T& new_value)	//编译器会自动优化移动语义
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


// 覆盖式线程安全环形队列
template<class T>
class ThreadSafe_Circular_Queue {
public:
    ThreadSafe_Circular_Queue(size_t max_size)
        : _buffer(max_size), _head(0), _tail(0), _size(0), _max_size(max_size)
    {}

    // 入队：满时自动覆盖最旧元素
    void push(const T& item)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (_size < _max_size) 
		{
            // 未满，直接添加
            _buffer[_tail] = item;
            _tail = (_tail + 1) % _max_size;
            ++_size;
        } 
		else 
		{
            // 已满，覆盖最旧元素（head 位置的元素）
            _buffer[_tail] = item;
            _tail = (_tail + 1) % _max_size;
            _head = (_head + 1) % _max_size;  // head 前进，丢弃最旧元素
            // _size 保持不变
        }
    }

    void push(T&& item)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (_size < _max_size) 
		{
            _buffer[_tail] = std::move(item);
            _tail = (_tail + 1) % _max_size;
            ++_size;
        } 
		else 
		{
            _buffer[_tail] = std::move(item);
            _tail = (_tail + 1) % _max_size;
            _head = (_head + 1) % _max_size;
        }
    }

    // 出队
    std::shared_ptr<T> pop()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_size == 0) 
		{
            return nullptr;
        }
        
        std::shared_ptr<T> res = std::make_shared<T>(std::move(_buffer[_head]));
        _head = (_head + 1) % _max_size;
        --_size;
        return res;
    }

    // 查看队首元素
    std::shared_ptr<T> front()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_size == 0) 
		{
            return nullptr;
        }
        return std::make_shared<T>(_buffer[_head]);
    }

    // 判空
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _size == 0;
    }

    // 当前元素数量
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _size;
    }

    // 容量
    size_t capacity() const
    {
        return _max_size;
    }

private:
    std::vector<T> _buffer;
    size_t _head;      // 队头索引（最旧元素）
    size_t _tail;      // 队尾索引（下一个写入位置）
    size_t _size;      // 当前元素数量
    const size_t _max_size;
    mutable std::mutex _mutex;
};

// 等待-通知模式下的线程安全覆盖式环形队列（阻塞版本）
template<class T>
class Wait_Notify_ThreadSafe_Circular_Queue {
public:
    Wait_Notify_ThreadSafe_Circular_Queue(size_t max_size)
        : _buffer(max_size), _head(0), _tail(0), _size(0), _max_size(max_size)
    {}

    // 入队：满时自动覆盖最旧元素，并通知等待的消费者
    template<typename U>
    void push(U&& item)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (_size < _max_size) 
		{
            _buffer[_tail] = std::forward<U>(item);
            _tail = (_tail + 1) % _max_size;
            ++_size;
        } 
		else 
		{
            // 已满，覆盖最旧元素
            _buffer[_tail] = std::forward<U>(item);
            _tail = (_tail + 1) % _max_size;
            _head = (_head + 1) % _max_size;
        }
        
        _cond.notify_one();  // 通知一个等待的消费者
    }

    // 阻塞式出队（引用版本）
    void pop(T& value)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock, [this]() {
            return _size > 0;
        });

        value = std::move(_buffer[_head]);
        _head = (_head + 1) % _max_size;
        --_size;
    }

    // 带超时的阻塞式出队
    template<typename Rep, typename Period>
    bool pop_wait_for(T& value, const std::chrono::duration<Rep, Period>& timeout)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_cond.wait_for(lock, timeout, [this]() {
            	return _size > 0;
        	})) 
		{
            value = std::move(_buffer[_head]);
            _head = (_head + 1) % _max_size;
            --_size;
            return true;
        }
        return false;  // 超时
    }

    // 非阻塞查看队首
    std::shared_ptr<T> front()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_size == 0) 
		{
            return nullptr;
        }
        return std::make_shared<T>(_buffer[_head]);
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _size == 0;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _size;
    }

    size_t capacity() const
    {
        return _max_size;
    }

private:
    std::vector<T> _buffer;
    size_t _head;
    size_t _tail;
    size_t _size;
    const size_t _max_size;
    mutable std::mutex _mutex;
    std::condition_variable _cond;
};