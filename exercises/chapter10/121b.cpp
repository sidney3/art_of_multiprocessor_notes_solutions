#include <atomic>
#include <cassert>
#include <memory>
#include <optional>
#include "test_pool.h"
#include <format>

/*
    This type of thing can never work because we can  always have a wraparound - that is, our queue is too small, one thread waits, and they
    get their value overwritten by another thread...

    Tail = 4, Head = 5

    Both covering.

    And so we make a write, and we think we own the index 5, but then we execute however many more writes so that we wrap around.
*/
template<typename T>
struct Entry
{
    template<typename ... Args>
    Entry(Args&&... args)
    : object{std::forward<Args>(args)...}, flag{false}
    {}
    T object;
    std::atomic<bool> flag;
};
template<typename T,
    typename Allocator = std::allocator<Entry<T>>>
struct LockFreeQueue
{
    LockFreeQueue(size_t capacity = 1024, const Allocator& alloc = Allocator{})
        : capacity_(capacity), allocator_(alloc),
        head_(0), tail_(0)
    {
        // make a note of this std::allocator_traits
        // it's a good pattern
        std::memset(empty_T, '\0', sizeof(Entry<T>));
        data_ = std::allocator_traits<Allocator>::allocate(
                allocator_, capacity_);

        std::memset(data_, '\0', capacity_ * sizeof(Entry<T>));
    }
    std::optional<T> deq(void)
    {
        long long localHead = head_.load(std::memory_order_acquire), nextHead;

        do
        {
            if(localHead == tail_.load(std::memory_order_acquire))
            {
                return std::nullopt;
            }
            nextHead = localHead + 1;

        } while(!head_.compare_exchange_weak(localHead, nextHead, std::memory_order_acq_rel));

        // we now own localHead

        while(!data_[localHead % capacity_].flag.load(std::memory_order_acquire))
        {
            /* std::cout << std::format("deq thread: {} spinning on index {}\n", std::this_thread::get_id(), localHead); */
        }

        /* assert(std::memcmp(&data_[localHead], empty_T, sizeof(Entry<T>)) != 0); */

        T res = data_[localHead % capacity_].object;
        data_[localHead % capacity_].object.~T();
        data_[localHead % capacity_].flag.store(false, std::memory_order_release);
        /* memset(&data_[localHead], '\0', sizeof(Entry<T>)); */

        /* std::cout << std::format("enq thread: {} succeeded on index {}\n", std::this_thread::get_id(), localHead); */

        return res;
    }
    template<typename ... Args>
    bool enq(Args&&... args)
    {
        long long localTail = tail_.load(std::memory_order_acquire), nextTail;

        do
        {
            nextTail = localTail + 1;

            if(nextTail == head_.load(std::memory_order_acquire))
            {
                return false;
            }

        } while(!tail_.compare_exchange_weak(localTail, nextTail, std::memory_order_acq_rel));

        while(data_[localTail % capacity_].flag.load(std::memory_order_acquire))
        {
            /* std::cout << std::format("thread enq: {} spinning on index {}\n", std::this_thread::get_id(), localTail); */
        }

        /* assert(std::memcmp(&data_[localTail], empty_T, sizeof(Entry<T>)) == 0); */

        new (&data_[localTail % capacity_].object) T {std::forward<Args>(args)...};
        data_[localTail % capacity_].flag.store(true, std::memory_order_release);

        /* std::cout << std::format("thread enq: {} succeeded on enq for index {}\n", std::this_thread::get_id(), localTail); */

        return true;
    }

    bool empty()
    {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }
    size_t size()
    {
        int res = static_cast<int>(tail_.load(std::memory_order_acquire)) 
            - static_cast<int>(head_.load(std::memory_order_acquire));

        if(res < 0)
        {
            res += capacity_;
        }

        assert(res > 0);

        return static_cast<size_t>(res);
    }
    
private:
    char empty_T[sizeof(T)];

    const size_t capacity_;
    Allocator allocator_;
    Entry<T>* data_;
    std::atomic<long long> head_;
    std::atomic<long long> tail_;
};



int main()
{
    LockFreeQueue<int> q;
    test_pool(q);
}
