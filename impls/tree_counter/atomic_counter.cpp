#include "test_counter.h"
#include <atomic>

struct AtomicCounter
{
    AtomicCounter(size_t num_threads)
    {}
    int getAndIncrement(size_t thread_id)
    {
        std::unique_lock lk{mtx};
        return ctr++;
    }

    std::mutex mtx;
    int ctr{0};

};

int main()
{
    test_counter<AtomicCounter>();
}
