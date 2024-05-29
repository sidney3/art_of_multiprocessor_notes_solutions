/*
   We decide to program the stated peterson lock for N nodes
*/

#include <memory>
#include <iostream>
#include <chrono>
#include <thread>
#include <cassert>

constexpr long MSB(long N)
{
    return (sizeof(long) * 8) - __builtin_clzl(N);
}
static_assert(MSB(1<<4) == 5);
static_assert(MSB((1<<4) + (1<<5) + (1<<7) + 1) == 8);

constexpr bool is_power_of_2(long N)
{
    return (1<<(MSB(N)-1)) == N;
}
static_assert(is_power_of_2(8));
static_assert(!is_power_of_2(9));

#include <thread>

struct PetersonLock
{
    std::vector<std::atomic<bool>> flags;
    std::atomic<int> victim;
    const size_t base_id;
    const size_t num_threads;

    PetersonLock(int n, size_t base_id) : flags(n), victim(), base_id(base_id),num_threads(n)
    {
        std::fill(flags.begin(), flags.end(), false);
    }

    void lock(size_t thread_id)
    {
        assert(thread_id - base_id < num_threads);
        flags[thread_id - base_id] = true;
        victim = thread_id;
        
        bool is_stopped = true;
        while(is_stopped)
        {
            is_stopped = false;

            for(size_t other_thread = base_id, top_thread = base_id + num_threads;
                    other_thread < top_thread;
                    other_thread++)
            {
                assert(other_thread - base_id < num_threads);
                is_stopped = is_stopped || (victim == thread_id && flags[other_thread - base_id]);
            }
        }

    }

    void unlock(size_t thread_id)
    {
        flags[thread_id - base_id].store(false, std::memory_order_relaxed);
    }
};

template<long N>
struct PetersonTreeLock
{
    static_assert(is_power_of_2(N));
    PetersonTreeLock()
    {
        init_node(1, 0, N - 1);
    }

    void lock(size_t thread_id)
    {
        size_t curr_node = *thread_id_to_leaf[thread_id];

        while(curr_node != root_node)
        {
            lock_tree[curr_node]->lock(thread_id);
            curr_node /= 2;
        }

        lock_tree[root_node]->lock(thread_id);

    }
    void unlock(size_t thread_id)
    {
        size_t curr_node = root_node;
        size_t leaf_node = *thread_id_to_leaf[thread_id];
        size_t curr_l = 0, curr_r = N - 1;

        while(curr_node != leaf_node)
        {
            lock_tree[curr_node]->unlock(thread_id);
            size_t m = (curr_r - curr_l)/2;
            if(thread_id < m)
            {
                curr_r = m - 1;
                curr_node *= 2;
            }
            else
            {
                curr_l = m;
                curr_node = (curr_node * 2) + 1;
            }
        }

        lock_tree[leaf_node]->unlock(thread_id);
    }


    // non-copyable and non-movable
    PetersonTreeLock(const PetersonTreeLock&) = delete;
    PetersonTreeLock(PetersonTreeLock&&) = delete;

private:
    static constexpr size_t root_node = 1;
    std::array<std::unique_ptr<PetersonLock>, 2*N + 1> lock_tree;
    std::array<std::unique_ptr<const size_t>, 2*N + 1> thread_id_to_leaf;


    void init_node(
            size_t my_idx,
            size_t range_start, 
            size_t range_end
            )
    {
        size_t range_size = range_end - range_start + 1;

        lock_tree[my_idx] = std::make_unique<PetersonLock>(range_size, range_start);

        if(range_size == 1)
        {
            thread_id_to_leaf[range_start] = std::make_unique<size_t>(my_idx);
            thread_id_to_leaf[range_end] = std::make_unique<size_t>(my_idx);
        }
        else
        {
            int m = (range_start + range_end) / 2;
            init_node(my_idx*2,
                      range_start,
                      range_start + m - 1
                      );
            init_node(my_idx*2 + 1,
                      m,
                      range_end);
        }
    }

};

template<long N>
void locking_thread(PetersonTreeLock<N>& mtx, size_t thread_id)
{
    while(1)
    {
        mtx.lock(thread_id);
        std::cout << "thread " << thread_id << " in the critical zone\n";
        std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(3));
        mtx.unlock(thread_id);
    }
}

int main()
{
    constexpr long N = 4;
    PetersonTreeLock<N> mtx;
    std::array<std::unique_ptr<std::thread>, N> threads;
    for(size_t i = 0; i < N; i++)
    {
        threads[i] = std::make_unique<std::thread>
            (locking_thread<N>, std::ref(mtx), i);
    }

    for(size_t i = 0; i < N; i++)
    {
        threads[i]->join();
    }

}
