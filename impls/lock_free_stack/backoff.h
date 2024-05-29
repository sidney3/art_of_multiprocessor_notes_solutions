#include <random>
#include <thread>
#include <chrono>

struct backoff
{
    backoff(size_t MIN_WAIT, size_t MAX_WAIT)
        : rng(std::random_device{}()), MIN_WAIT(MIN_WAIT),
        MAX_WAIT(MAX_WAIT), curr_ub(MIN_WAIT)
    {}

    void operator()()
    {
        std::this_thread::sleep_for(
                std::chrono::milliseconds{std::uniform_int_distribution<size_t>{MIN_WAIT, curr_ub}(rng)});
        curr_ub = std::min(MAX_WAIT, 2*curr_ub);
    }
    void reset()
    {
        curr_ub = MIN_WAIT;
    }
private:
    std::mt19937 rng; 
    size_t MIN_WAIT;
    size_t MAX_WAIT;
    size_t curr_ub;
    
};
