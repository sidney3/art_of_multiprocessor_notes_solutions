#pragma once
#include <cassert>
#include <functional>
#include <thread>
#include <numeric>
#include <algorithm>
#include <iostream>

using namespace std::placeholders;
/*
    Our testing strategy for an arbitrary pool:

    We have a bunch of writer threads and reader threads.

    The writers write a bunch of values, keeping track of the frequency of each element added
    The readers read all values and keep their own track of frequency

    Each writer adds a fixed amount of data to the pool
*/

#if defined(if_debug)
    // already defined, no need to redefine
#elif defined(debug) && debug
    #define if_debug(x) (x)
#else
    #define if_debug(x) 
#endif

template<typename Map>
Map merge_maps(Map&& m1, Map& m2)
{
    Map res;

    for(auto &[k,v] : m1)
    {
        res.insert({k,v});
    }
    for(auto &[k,v] : m2)
    {
        auto it = res.find(k);

        if(it == res.end())
        {
            res.insert({k,v});
        }
        else
        {
            res[k] = it->second + v;
        }
    }

    return res;
}
template<typename Pool>
void test_pool(Pool&& p)
{
    using int_map = std::unordered_map<size_t,size_t>;

    auto writer_thread = [](Pool p, int_map& my_freqs, size_t vals_to_write)
    {
        for(size_t i = 0; i < vals_to_write; ++i)
        {
            int to_add = rand();
            if(to_add == 0)
            {
                continue;
            }

            int freq = 0;
            while(!p.enq(to_add))
            {
                freq++;
                assert(freq < 1e8);
            }

            (my_freqs)[to_add]++;
        }
        if_debug(std::cout << std::format("thread {} finished writing\n", std::this_thread::get_id()));
    };

    auto reader_thread= [](Pool p, std::atomic<bool>& signal, int_map& my_freqs)
    {
        while(signal.load(std::memory_order_acquire) || !p.empty())
        {
            auto val = p.deq();
            if(val)
            {
                (my_freqs)[*val]++;
            }
        }
        
        if_debug(std::cout << std::format("thread {} finished reading\n", std::this_thread::get_id()));
    };

    const size_t readers = 20, writers = 20, vals_to_write = 10000;
    std::vector<int_map> reader_maps(readers), writer_maps(writers);
    std::vector<std::thread> reader_threads, writer_threads;
    std::atomic<bool> read_signal{ true };

    for(size_t i = 0; i < writers; i++)
    {
        writer_threads.emplace_back(
                writer_thread,
                std::ref(p), 
                std::ref(writer_maps[i]), 
                vals_to_write);
    }

    for(size_t i = 0; i < readers; i++)
    {
        reader_threads.emplace_back(
                reader_thread,
                std::ref(p),
                std::ref(read_signal),
                std::ref(reader_maps[i])
                );
    }

    /* std::this_thread::sleep_for(std::chrono::milliseconds(100)); */

    for(auto& th : writer_threads)
    {
        th.join();
    }

    read_signal.store(false, std::memory_order_release);

    for(auto& th : reader_threads)
    {
        th.join();
    }

    auto read_freqs = std::accumulate(
        reader_maps.begin(),
        reader_maps.end(),
        int_map{},
        merge_maps<int_map>);
    
    
    auto write_freqs = std::accumulate(
        writer_maps.begin(),
        writer_maps.end(),
        int_map{},
        merge_maps<int_map>);

    assert(read_freqs.size() == write_freqs.size());
    
    
    for(auto &[k,v] : read_freqs)
    {
        auto it = write_freqs.find(k);
    
        assert(it != write_freqs.end() &&
                "value not found");
    
        assert(v == it->second &&
                "frequency is not the same");
    }

}
