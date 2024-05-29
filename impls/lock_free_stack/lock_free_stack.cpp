#include "lock_free_exchanger.h"
#include "../StampedAllocator/StampedAllocator.h"
#include "../../exercises/chapter10/test_pool.h"
#include <optional>
#include "thread_safe_alloc.h"
#include <atomic>


template<typename U>
using MyStampedRef = StampedRefNormal<U>;

template<typename T>
struct StackNode
{
    StackNode(T val, MyStampedRef<StackNode<T>> _prev = nullptr)
        : value(val), prev(_prev)
    {}
    T value;
    MyStampedRef<StackNode<T>> prev;    
};

template<typename T, typename Allocator = ThreadSafeAllocator<StackNode<T>>>
struct lock_free_stack
{
    lock_free_stack()
        : head{nullptr}, alloc{}, arr{}
    {
    }
    template<typename ... Args>
    bool enq(Args&&... args)
    {
        MyStampedRef<StackNode<T>> proposedNode = alloc.allocate();
        
        new (proposedNode.get_ptr()) StackNode<T>{T{std::forward<Args>(args)...}};
        while(true)
        {
            if(try_enq(proposedNode))
            {
                return true;
            }
            /* size_t myIndex = exchangeGive.fetch_add(1); */
            if(arr.deliver(proposedNode, std::chrono::system_clock::now() + delay))
            {
                return true;
            }
            std::this_thread::yield();
            /* else */
            /* { */
            /*     exchangeGive.fetch_sub(1); */
            /* } */
        }
    }
    std::optional<T> deq(void)
    {
        while(true)
        {
            if(head.load(std::memory_order_acquire) == nullptr)
            {
                return std::nullopt;
            }

            auto maybeRes = try_deq();

            if(maybeRes.has_value())
            {
                T res = (*maybeRes)->value;
                alloc.deallocate((*maybeRes));
                return res;
            }

            // the issue is with the exchanging. Something is not working
            /* size_t myIndex = exchangeTake.fetch_add(1); */
            maybeRes = arr.receive(std::chrono::system_clock::now() + delay);
            if(maybeRes.has_value())
            {
                T res = (*maybeRes)->value;
                alloc.deallocate((*maybeRes));
                return res;
            }

            std::this_thread::yield();
            /* else */
            /* { */
            /*     exchangeTake.fetch_sub(1); */
            /* } */
        }
    }
    bool empty() const
    {
        return head.load() == nullptr;
    }
private:
    std::optional<MyStampedRef<StackNode<T>>> try_deq(void)
    {
        auto localHead = head.load(std::memory_order_acquire);
        if(localHead == nullptr)
        {
            return std::nullopt;
        }
        auto prevHead = localHead->prev;

        if(head.compare_exchange_strong(localHead, prevHead))
        {
            return std::make_optional<MyStampedRef<StackNode<T>>>(std::move(localHead));
        }
        return std::nullopt;
    }
    bool try_enq(MyStampedRef<StackNode<T>> node)
    {
        auto localHead = head.load(std::memory_order_acquire);
        node->prev = localHead;
        return head.compare_exchange_strong(localHead, node);
    }
    std::atomic<size_t> exchangeGive = 0;
    std::atomic<size_t> exchangeTake = 0;
    std::atomic<MyStampedRef<StackNode<T>>> head = nullptr;
    Allocator alloc;
    static constexpr auto delay = std::chrono::milliseconds{25};
    ExchangerArray<MyStampedRef<StackNode<T>>, 20> arr;
};


int main()
{
    lock_free_stack<int> S;
    test_pool(S);
}
