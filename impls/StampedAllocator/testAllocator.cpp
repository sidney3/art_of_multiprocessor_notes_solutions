#include "StampedAllocator.h"
#include "../../exercises/chapter10/test_pool.h"
#include "../lock_free_stack/thread_safe_alloc.h"
#include "../lock_free_stack/backoff.h"

#define true_cast(x) true


template<typename T>
using StampedRef = StampedRefNormal<T>;

template<typename T>
struct StackNode
{
    StackNode(T item, StampedRef<StackNode<T>> ptr = nullptr)
        : item(item), prev(ptr)
    {}
    T item;
    StampedRef<StackNode<T>> prev;
};


template<typename T>
struct ConcurrentStack
{
    ThreadSafeAllocator<StackNode<T>> allocator;
    ConcurrentStack()
        : head{nullptr}
    {
    }
    std::optional<T> deq()
    {
        StampedRef<StackNode<T>> localHead = head.load(std::memory_order_acquire);
        if(localHead == nullptr)
        {
            return std::nullopt;
        }
        StampedRef<StackNode<T>> localPrev  = localHead->prev;

        while(!head.compare_exchange_strong(localHead, localPrev))
        {
            backoff();
            if(localHead == nullptr)
            {
                return std::nullopt;
            }
            localPrev = localHead->prev;
        }

        backoff.reset();
        T res = localHead->item;
        allocator.deallocate(localHead);

        return res;

    }
    template<typename ... Args>
    bool enq(Args&&... args)
    {
        StampedRef<StackNode<T>> new_node = allocator.allocate();
        new (new_node.get_ptr()) StackNode<T>{T{std::forward<Args>(args)...}};

        while(!try_push(new_node))
        {
            backoff();
        }

        backoff.reset();

        return true;
    }
    bool empty()
    {
        return head.load() == nullptr;
    }
private:
    bool try_push(StampedRef<StackNode<T>> node) 
    {
        auto localHead = head.load(std::memory_order_acquire);
        node->prev = localHead;
        return head.compare_exchange_strong(localHead, node);
    }
    std::atomic<StampedRef<StackNode<T>>> head;
    static thread_local backoff backoff;
};

template<>
thread_local backoff ConcurrentStack<int>::backoff{2, 250};

int main()
{
    ConcurrentStack<int> s;
    test_pool(s);
}

