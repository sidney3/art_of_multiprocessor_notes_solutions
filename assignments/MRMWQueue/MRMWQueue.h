#include <atomic>
#include <array>
#include <utility>
#include <iostream>

/*
    Naive MRMW Queue

    To add / read, we do a CAS on the current head / tail index. If this succeeds, we "own" that index, either as a reader or as a writer.
*/

template<typename T>
struct MRMWQueue
{
public:
    MRMWQueue(T init = T{}) 
    : MRMWQueue(init, std::make_index_sequence<capacity>())
    {}

    void push(T value)
    {
        size_t localHead = head.load();
        size_t nextHead;

        do
        {
            nextHead = localHead + 1;

            if(nextHead == capacity)
            {
                nextHead = 0;
            }

            while(nextHead == tail.load())
            {
                /* std::cout << "locked out in push!\n"; */
            }

        } while(!head.compare_exchange_strong(localHead, nextHead));

        queue[localHead] = value;
    }

    T pop()
    {
        size_t localTail = tail.load();
        size_t nextTail;

        do
        {
            nextTail = localTail + 1;

            if(nextTail == capacity)
            {
                nextTail = 0;
            }

            while(nextTail == head.load())
            {
                std::cout << "locked out in pop!\n";
            }

        } while(!tail.compare_exchange_strong(localTail, nextTail));

        return queue[localTail];
    }
    size_t size() const
    {
        size_t localHead = head.load();
        size_t localTail = tail.load();

        if(localHead >= localTail)
        {
            return localHead - localTail;
        }
        else
        {
            return capacity - localTail + localHead;
        }
    }
    bool empty() const
    {
        size_t localHead = head.load();
        size_t localTail = tail.load();
        return localHead == localTail;
    }
private:
    static constexpr size_t capacity = 1024;
    std::array<T,capacity> queue;
    std::atomic<size_t> tail;
    std::atomic<size_t> head;

    template<size_t ... Is>
    MRMWQueue(T init, std::index_sequence<Is...>)
        : queue{ (static_cast<void>(Is), init)... }, tail(0), head(0)
    {}
};
