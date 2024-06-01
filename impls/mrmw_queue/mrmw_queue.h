/*
    Algorithm for the MRMW queue:

    Single base array. Each entry contains an (optional) value and a turn value

    The turn value is even if the slot is empty, and odd if the slot is even.

    turn / 2 gives the "cycle" that we are currently considering: that is, if turn = 2,
    we know that we are currently empty and on cycle 1. The cycle number indicates the
    cycle of the head that we are supporting.

    In general we have issues with wraparound (two threads have head values that refer to
    the same index) and so this offers priority between them.

    Heavy inspiration from Eric Rigtorps MCMP Queue
*/
#include <atomic>
#include <vector>
#include <optional>
#include <format>
#include <new>
#include "../../exercises/chapter10/test_pool.h"

#if defined(if_debug)
    // already defined, no need to redefine
#elif defined(debug) && debug
    #define if_debug(x) (x)
#else
    #define if_debug(x) 
#endif


template<typename T>
struct queue_traits
{
    static T empty_value();
};

template<>
struct queue_traits<int>
{
    static int empty_value() { return -1; }
};

template<typename T>
struct slot
{
    slot()
        : turn {0}, 
        item {queue_traits<T>::empty_value()}
    {}
    void destroy()
    {
        item.~T();
    }
    std::atomic<size_t> turn;
    T item;
};

template<typename T>
struct MRMWQueue
{
    MRMWQueue(size_t capacity)
        : capacity_(capacity), data_(capacity)
    {}

    template<typename ... Args>
    void force_enq(Args&&... args)
    {
        auto localHead = head_.fetch_add(1);

        while(!(turn(localHead)*2 == data_[idx(localHead)].turn.load()));

        new (&data_[idx(localHead)].item) T{std::forward<Args>(args)...};

        data_[idx(localHead)].turn.store(turn(localHead)*2 + 1);
    }

    template<typename ... Args>
    bool enq(Args&&... args)
    {
        auto localHead = head_.load(std::memory_order_acquire);

        while(true)
        {
            if(turn(localHead)*2 == data_[idx(localHead)].turn.load())
            {
                if(head_.compare_exchange_strong(localHead, localHead + 1))
                {
                    new (&data_[idx(localHead)].item) T{std::forward<Args>(args)...};
                    data_[idx(localHead)].turn.store(turn(localHead)*2 + 1);
                    if_debug(std::cout << std::format("thread {}: successful enq\n", std::this_thread::get_id()));
                    return true;
                }
            }
            else
            {
                // TODO: test with just returning false here
                /* return false; */
                auto nextHead = head_.load(std::memory_order_acquire);
                
                if(localHead == nextHead)
                {
                    return false;
                }
                localHead = nextHead;
            }
        }
    }

    T force_deq()
    {
        auto localTail = tail_.fetch_add(1);

        while(!(turn(localTail)*2 + 1 == data_[idx(localTail)].turn.load()));

        T res = std::move(data_[idx(localTail)].item);
        data_[idx(localTail)].destroy();
        data_[idx(localTail)].turn.store(turn(localTail)*2 + 2);

        return res;
    }

    std::optional<T> deq()
    {
        auto localTail = tail_.load(std::memory_order_acquire);

        while(true)
        {
            if(turn(localTail)*2 + 1 == data_[idx(localTail)].turn.load())
            {
                if(tail_.compare_exchange_strong(localTail, localTail + 1))
                {
                    auto res = 
                        std::make_optional<T>(
                                std::move(data_[idx(localTail)].item)
                                );
                    data_[idx(localTail)].destroy();
                    data_[idx(localTail)].turn.store(turn(localTail)*2 + 2);
                    if_debug(std::cout << std::format("thread {}: successful deq\n", std::this_thread::get_id()));
                    return res;
                }
            }
            else
            {
                /* return std::nullopt; */
                auto nextTail = tail_.load();
                if(nextTail == localTail)
                {
                    return std::nullopt;
                }
                localTail = nextTail;
            }
        }
    }

    bool empty() const
    {
        return head_.load() == tail_.load();
    }

private:

    static constexpr size_t cacheLineSize = 64;

    size_t idx(size_t i) const { return i % capacity_; }

    size_t turn(size_t i) const { return i / capacity_; }

    const size_t capacity_;
    std::vector<slot<T>> data_;
    alignas(cacheLineSize) std::atomic<size_t> head_{ 0 };
    alignas(cacheLineSize) std::atomic<size_t> tail_{ 0 };
};
