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

void philosopher_thread(std::array<chopstick, NUM_CHOPSTICKS>& chopsticks, size_t index)
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
        std::cout << "Philosopher number " << index << " has started searching for a chopstick\n";

        std::optional<chopstick*> c1 = std::nullopt, c2 = std::nullopt;

        while(!(c1.has_value()))
        {
            c1 = search_chopsticks();
        }

        std::cout << "Philosopher number " << index << " has acquired their first chopstick\n";
        while(!(c2.has_value()))
        {
            c2 = search_chopsticks();
        }
        std::cout << "Philosopher number " << index << " has acquired their second chopstick\n";

        (**c1).m.unlock();
        (**c2).m.unlock();

        std::cout << "Philosopher number " << index << " has released their chopsticks and resumed studying\n";
        std::this_thread::sleep_for(std::chrono::duration<long, std::milli>(1));
    }
}


int main()
{

    std::array<chopstick, NUM_CHOPSTICKS> chopsticks;
    std::vector<std::thread> philosopher_threads;


    for(size_t i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        philosopher_threads.emplace_back(philosopher_thread, std::ref(chopsticks), i);
    }

    for(auto& th : philosopher_threads)
    {
        th.join();
    }

    
}
