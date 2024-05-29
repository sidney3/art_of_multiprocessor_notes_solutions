/*

   The basic idea behind the non-starving approach is that we will want the following condition to be true:

   whenever the philosopher who has eaten the fewest amount of times tries to eat, he will succeed.

   We will get this with the following behavior: when we queue up to get rice, we use the same strategy as before
*/

#include <iostream>
#include <thread>
#include <mutex>

#define NUM_CHOPSTICKS 5
#define NUM_PHILOSOPHERS 5

struct chopstick
{
    chopstick() : m()
    {}
    std::mutex m;

    // can't copy a chopstick!
    chopstick(const chopstick& copy) = delete;
};

void philosopher_thread(
        std::array<std::atomic<int>, NUM_PHILOSOPHERS>& feed_count,
        std::array<chopstick, NUM_CHOPSTICKS>& chopsticks,
        size_t index)
{
    while(1)
    {
        
        // returns a locked chopstick
        auto search_chopsticks = [i = index, &chopsticks]() -> std::optional<chopstick*>
        {
            for(size_t search_dis = 0; search_dis < NUM_CHOPSTICKS; search_dis++)
            {
                size_t i1 = (i + search_dis) % NUM_CHOPSTICKS;
                size_t i2 = (i - search_dis + NUM_CHOPSTICKS) % NUM_CHOPSTICKS;


                if(chopsticks[i1].m.try_lock())
                {
                    return std::make_optional(&(chopsticks)[i1]);
                }
                if(chopsticks[i2].m.try_lock())
                {
                    return std::make_optional(&chopsticks[i2]);
                }
            }
            return std::nullopt;
        };

        auto is_lowest = [i = index, &feed_count]() -> bool
        {
            int my_count = feed_count[i].load();
            bool is_lowest_res = true;

            for(size_t philo_i = 0; philo_i < NUM_PHILOSOPHERS && is_lowest_res; philo_i++)
            {
                if(philo_i == i)
                {
                    continue;
                }

                int other_count = feed_count[philo_i].load();

                is_lowest_res = other_count < my_count || (other_count == my_count && philo_i < i);
            }
            return is_lowest_res;
        };
        std::cout << "Philosopher number " << index << " has started searching for a chopstick\n";

        auto c1 = search_chopsticks(), c2 = search_chopsticks();

        while(!(c1.has_value() && c2.has_value()) && is_lowest())
        {
            if(c1.has_value())
            {
                (**c1).m.unlock();
            }
            if(c2.has_value())
            {
                (**c2).m.unlock();
            }
            c1 = search_chopsticks();
            c2 = search_chopsticks();
        }

        if(c1.has_value() && c2.has_value())
        {
            std::cout << "Philosopher number " << index << " has found both of their chopsticks\n";
            feed_count[index]++;
        }
        else
        {
            std::cout << "Philosopher number " << index << " has not found their chopsticks\n";
        }

        if(c1.has_value())
        {
            (**c1).m.unlock();
        }
        if(c2.has_value())
        {
            (**c2).m.unlock();
        }
        std::this_thread::sleep_for(std::chrono::duration<long, std::milli>(1));
    }
}


int main()
{

    std::array<chopstick, NUM_CHOPSTICKS> chopsticks;
    std::array<std::atomic<int>, NUM_PHILOSOPHERS> philosopher_count;
    std::vector<std::thread> philosopher_threads;


    for(size_t i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        philosopher_threads.emplace_back(philosopher_thread, std::ref(philosopher_count), std::ref(chopsticks), i);
    }

    for(auto& th : philosopher_threads)
    {
        th.join();
    }

    
}
