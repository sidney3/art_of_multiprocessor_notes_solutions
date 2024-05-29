#include "MRMWQueue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
using namespace std::chrono;
using duration_type = duration<long long, std::nano>;

/*
results for 10threads: 
average read: 134ns average write: 184ns
*/

struct thread_data
{
    size_t total_reads;
    size_t total_writes;
    duration_type total_read_time;
    duration_type total_write_time;
};

void reader_thread(size_t reads_to_make, MRMWQueue<int>& q, thread_data& write_to)
{
    size_t reads_made = 0;
    duration_type total_read_time{0}, total_write_time{0};
    while(reads_made < reads_to_make)
    {
        auto start = system_clock::now();
        int popped = q.pop();
        auto after_pop = system_clock::now();
        q.push(popped);
        auto after_push = system_clock::now();

        ++reads_made;
        auto dur1 = 
            duration_cast<duration_type>(after_pop - start);
        auto dur2 = 
            duration_cast<duration_type>(after_push - after_pop);
        total_read_time += dur1;
        total_write_time += dur2;
    }
    write_to = {reads_made, reads_made, total_read_time, total_write_time};
}
void test_queue(size_t num_threads)
{
    MRMWQueue<int> q{ 0 };

    for(int i = 0; i < 200; i++)
    {
        q.push(i);
    }

    std::cout << "finished pushing elements\n";
    std::vector<std::thread> threads;
    std::vector<thread_data> thread_results;

    size_t total_reads{0}, total_writes{0};
    duration_type read_time{0}, write_time{0};


    for(size_t i = 0; i < num_threads; i++)
    {
        thread_results.emplace_back();
        std::thread th{reader_thread, 100, std::ref(q), std::ref(thread_results[i])};
        threads.push_back(std::move(th));
    }
    for(size_t i = 0; i < num_threads; i++)
    {
        threads[i].join();
        total_reads += thread_results[i].total_reads;
        total_writes += thread_results[i].total_writes;
        std::cout << thread_results[i].total_reads << " " << thread_results[i].total_writes << "\n";
    }
    std::cout << total_reads << " " << total_writes << "\n";

    duration_type average_read;
    duration_type average_write;
    for(size_t i = 0; i < num_threads; i++)
    {
        auto new_reads = thread_results[i].total_read_time / total_reads;
        auto new_writes = thread_results[i].total_read_time / total_reads;

        average_read += new_reads;
        average_write += new_writes;

        std::cout << average_write.count() << "\n";
    }



    std::cout << "results for " << num_threads << "threads: \n";
    std::cout << "average read: " << average_read.count() << "ns average write: " << average_write.count() << "ns\n";
}

void test_various_thread_sizes()
{
    for(size_t num_threads = 1; num_threads <= 126; num_threads += 25)
    {
        test_queue(num_threads);
    }
}

int main()
{
    test_queue(10);
}
