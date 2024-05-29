#include <iostream>
#include <atomic>
#include <cassert>
#include "test_pool.h"

/*
Design a bounded lock-based and then lock free queue
*/

template<
    typename T, 
    typename Allocator = std::allocator<T>>
struct LockingQueue
{
    explicit LockingQueue(size_t capacity = 1024, const Allocator& allocator = Allocator{}) :
        _capacity(capacity),
        _allocator{allocator},
        enqLock{}, deqLock{},
        head{0}, tail{0}
    {
        _data = std::allocator_traits<Allocator>::allocate(
            _allocator, _capacity);
    }

    template<typename ... Args>
    [[nodiscard]]
    bool enq(Args&& ... args)
    {
        std::unique_lock<std::mutex> lk{enqLock};
        const auto tailIndex = tail.load(std::memory_order_acquire);
        
        auto nextTailIndex = tailIndex + 1;
        if(nextTailIndex == _capacity)
        {
            nextTailIndex = 0;
        }

        if(nextTailIndex == head.load(std::memory_order_acquire))
        {
            return false;
        }

        new (&_data[tailIndex]) T(std::forward<Args>(args)...);

        tail.store(nextTailIndex, std::memory_order_release);
        return true;
    }

    [[nodiscard]]
    std::optional<T> deq(void)
    {
        std::unique_lock<std::mutex> lk{deqLock};

        const auto headIndex = head.load(std::memory_order_acquire);

        if(headIndex == tail.load(std::memory_order_acquire))
        {
            return std::nullopt;
        }

        T res = _data[headIndex];
        _data[headIndex].~T();

        auto nextHeadIndex = headIndex + 1;
        if(nextHeadIndex == _capacity)
        {
            nextHeadIndex = 0;
        }

        head.store(nextHeadIndex, std::memory_order_release);
        return res;
    }

    [[nodiscard]]
    size_t size()
    {
        int res = tail.load(std::memory_order_acquire) - head.load(std::memory_order_acquire);
        if(res < 0)
        {
            res += _capacity;
        }

        return static_cast<size_t>(res);
    }


private:
    size_t _capacity;
    Allocator _allocator;
    T* _data;
    std::mutex enqLock;
    std::mutex deqLock;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
};

int main()
{
    LockingQueue<int> q;
    test_pool(q);
}
