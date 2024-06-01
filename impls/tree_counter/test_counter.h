#pragma once
#include "../mrmw_queue/mrmw_queue.h"
#include <chrono>
#include <thread>

#ifndef debug
    #define debug 1
#endif

#if defined(if_debug)
    // already defined, no need to redefine
#elif defined(debug) && debug
    #define if_debug(x) (x)
#else
    #define if_debug(x) 
#endif

template<typename Counter>
void counting_thread(Counter& c, MRMWQueue<int>& logger, const size_t thread_id, const size_t count_to)
{
    if_debug(std::cout << std::format("thread {}: starting counting\n", std::this_thread::get_id()));
    for(size_t i = 0; i < count_to; ++i)
    {
        int x = c.getAndIncrement(thread_id);
        if_debug(std::cout << std::format("thread {}: {}\n", std::this_thread::get_id(), x));
        logger.force_enq(x);
    }
    if_debug(std::cout << std::format("thread {} finished counting\n", std::this_thread::get_id()));
}

template<typename Counter>
void test_counter()
{
    
    if_debug(std::cout << "adf\n");
    static constexpr size_t counters = 10000, count_to = 100;

    Counter c(counters);

    std::vector<std::thread> threads;
    MRMWQueue<int> logger(count_to * counters + 1);

    auto start_time = std::chrono::system_clock::now();

    for(size_t i = 0; i < counters; ++i)
    {
        threads.emplace_back(counting_thread<Counter>, std::ref(c),
            std::ref(logger), i, count_to);
    }
    for(size_t i = 0; i < counters; ++i)
    {
        threads[i].join();
    }
    auto end_time = std::chrono::system_clock::now();

    std::vector<int> counted_values;
    while(!logger.empty())
    {
        counted_values.push_back(logger.force_deq());
    }

    std::sort(counted_values.begin(), counted_values.end());

    counted_values.erase(std::unique(counted_values.begin(), 
                counted_values.end()), counted_values.end());

    assert(counted_values.size() == count_to * counters &&
            counted_values[0] == 0);

    auto total_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    auto time_per_add = (total_time / (count_to * counters)).count();


    std::cout << std::format("timer counter passed with {}ns per count for {} counting threads up to {}", 
            time_per_add, counters,
            counters * count_to);
    
}
