#pragma once
#include <atomic>
#include <optional>
#include <chrono>
#include <random>
#include "../StampedAllocator/StampedAllocator.h"

using namespace std::chrono;


template<typename T>
struct Exchanger
{
    template<typename U>
    using StampedRef = StampedRefNormal<U>; 

    Exchanger()
    {}
    std::optional<T> receive(system_clock::time_point try_until)
    {
        while(system_clock::now() < try_until)
        {
            auto local = curr_value.load(std::memory_order_acquire);

            if(local.getStamp() != FULL)
            {
                continue;
            }

            if(curr_value.compare_exchange_strong(local, emptyNode, std::memory_order_seq_cst))
            {
                T res = *local.get_ptr();
                delete local.get_ptr();
                return res;
            }
        }

        return std::nullopt;
    }
    bool deliver(T& value, system_clock::time_point try_until)
    {
        T* to_deliver = new T{value};
        StampedRef<T> proposed{to_deliver, FULL};

        while(system_clock::now() < try_until)
        {
            auto local = curr_value.load(std::memory_order_acquire);

            if(local.getStamp() != EMPTY)
            {
                continue;
            }

            if(curr_value.compare_exchange_strong(local, proposed, std::memory_order_seq_cst))
            {
                while(system_clock::now() < try_until)
                {
                    if(curr_value.load().getStamp() == EMPTY)
                    {
                        return true;
                    }
                }
                // we've failed to perform an exchange
                // do a CAS to set the current node to an emptyNode
                // but if this happens to succeed then we return as such
                if(!curr_value.compare_exchange_strong(proposed, emptyNode, std::memory_order_seq_cst))
                {
                    return true;
                }
            }
        }
        return false;
    }
private:
    static constexpr long EMPTY = 0, FULL = 1;
    const StampedRef<T> emptyNode = {nullptr, EMPTY};
    std::atomic<StampedRef<T>> curr_value = emptyNode;
};

template<typename T, size_t N = 8>
struct ExchangerArray
{
    ExchangerArray() : rng(std::random_device{}()), distribution(0, N - 1)
    {}

    std::optional<T> receive(system_clock::time_point try_until)
    {
        return arr[distribution(rng)].receive(try_until);
    }
    std::optional<T> receive(size_t idx, system_clock::time_point try_until)
    {
        return arr[idx % N].receive(try_until);
    }
    bool deliver(size_t idx, T& value, system_clock::time_point try_until)
    {
        return arr[idx % N].deliver(value, try_until);
    }
    bool deliver(T& value, system_clock::time_point try_until)
    {
        return arr[distribution(rng)].deliver(value, try_until);
    }
    

private:
    std::mt19937 rng;
    std::uniform_int_distribution<size_t> distribution;
    Exchanger<T> arr[N];
};
