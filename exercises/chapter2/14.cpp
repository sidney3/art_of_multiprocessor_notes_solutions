#include <array>
#include <iostream>
#include <thread>
#include <cassert>
#include <chrono>

/*
   The P-exclusion problem is a variant of the starvation free mutual exclusion problem. 

   It has the following changes:

   At any time, as most P threads are in the critical section
   Fewer than P threads might fail (by halting) in the critical section

   We need n-exclusion and n-starvation freedom: as long as fewer than P threads are in the critical section,
   then some thread that wants to enter the critical section can (even if there are threads in the critical section
   that have halted).

   Modify the filter protocol to accomplish this. Basically, we should now, rather than allowing 1 thread in the final layer
   (which represents the critical section), we should allow P threads in the final layer

   Take the simple case of P = 2, N = 3

   Level 0: 3 threads
   Level 1: 2 threads
*/


using namespace std::chrono;

struct atomic_max
{
    std::mutex m;
    int curr_max;

    atomic_max(int init = 0) : m(), curr_max(init)
    {}
    void set(int x)
    {
        std::unique_lock<std::mutex> lk(m);
        curr_max = std::max(x, curr_max);
    }
    int get()
    {
        std::unique_lock<std::mutex> lk(m);
        return curr_max;
    }
};

template<long P, long N>
class PExclusionFilter
{
    static_assert(P <= N, "You cannot filter for more threads than exist");
public:

    PExclusionFilter() 
    {
        std::fill(thread_level.begin(), thread_level.end(), 0);
    }

    void lock(const int my_thread_id)
    {
        for(int level = 1; level <= num_levels; level++)
        {
            thread_level[my_thread_id] = level;

            victim[level] = my_thread_id;

            bool is_stopped = true;
            while(is_stopped)
            {
                is_stopped = false;
                for(int thread_id = 0; thread_id < N; thread_id++)
                {
                    if(thread_id == my_thread_id)
                    {
                        continue;
                    }

                    is_stopped = is_stopped 
                        || (victim[level] == my_thread_id 
                            && thread_level[thread_id] >= level);
                }
            }
        }
    }
    void unlock(const int my_thread_id)
    {
        thread_level[my_thread_id] = (0);
    }
private:
    static constexpr int num_levels = N - P;
    std::array<std::atomic<int>, N> thread_level;
    std::array<std::atomic<int>, num_levels + 1> victim;
};

template<long P, long N>
void test_PExclusionFilterThread(size_t thread_index, 
        PExclusionFilter<P,N>& mtx, std::atomic<int>& crit_count,
        time_point<system_clock> to_stop)
{
    while(system_clock::now() < to_stop)
    {
        std::this_thread::sleep_for(10ns);
        mtx.lock(thread_index);
        crit_count++;
        int crit_threads = crit_count.load();

        assert(crit_threads <= P && "More than P threads in critical zone!");

        crit_count--;

        mtx.unlock(thread_index);
    }
}

template<long AllowedThreads, long TotalThreads>
int test_PExclusionFilter(duration<int, std::milli> test_duration)
{
    PExclusionFilter<AllowedThreads,TotalThreads> mtx{};
    std::atomic<int> crit_count = 0;
    atomic_max max_crit_count(0);

    std::vector<std::thread> threads_store;
    threads_store.reserve(TotalThreads);

    time_point stop_time = system_clock::now() + test_duration;

    for(size_t i = 0; i < TotalThreads; i++)
    {
        threads_store.emplace_back(
                [&crit_count, &max_crit_count, &mtx, stop_time]
                (size_t thread_index)
                {
                    while(system_clock::now() < stop_time)
                    {
                        mtx.lock(thread_index);
                        crit_count++;
                        int crit_threads = crit_count.load();
                        max_crit_count.set(crit_threads);

                        assert(crit_threads <= AllowedThreads && "more than p threads in critical zone!");

                        crit_count--;

                        mtx.unlock(thread_index);
                    }
                }, i);
    }

    for(auto& th : threads_store)
    {
        th.join();
    }

    return max_crit_count.get();
}

int main()
{
    auto dur = duration<int, std::milli>(1000);
    std::cout << "maximum of: " << test_PExclusionFilter<10, 10>(dur) << "\n";
    std::cout << "maximum of: " << test_PExclusionFilter<10, 20>(dur) << "\n";
    std::cout << "maximum of: " << test_PExclusionFilter<10, 30>(dur) << "\n";
    std::cout << "maximum of: " << test_PExclusionFilter<10, 100>(dur) << "\n";

    // interesting note: for the higher number of threads, we have so much more difficulty getting through the threads 
    // that we never get to more than 1
}
