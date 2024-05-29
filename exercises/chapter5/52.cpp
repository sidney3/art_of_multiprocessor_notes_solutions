#include <atomic>
#include <array>
#include <cassert>

template<long N>
struct BinaryConsensus
{
    bool decide(bool b) {return false;}
};

template<long N>
struct NAryConsensus
{
    std::array<std::atomic<int>, N> proposed;
    std::array<BinaryConsensus<N>, N> consensuses;
    static constexpr int EMPTY_VAL = -1;

    NAryConsensus() : NAryConsensus(std::make_index_sequence<N>())
    {}

    void propose(size_t thread_id, int val)
    {
        assert(val >= 0 && val < N);
        proposed[thread_id].load(val, std::memory_order_acquire);
    }

    int decide(size_t thread_id, int val)
    {
        propose(thread_id, val);

        for(size_t i = 0; i < N; i++)
        {
            if(proposed[i] != EMPTY_VAL)
            {
                bool response = consensuses[i].propose(1);
                if(response)
                {
                    return proposed[i];
                }
            }
            else
            {
                bool response = consensuses[i].propose(0);
                if(response)
                {
                    return proposed[i];
                }
            }
        }

        return -1;
    }
private:
    template<size_t... Is>
    NAryConsensus(std::index_sequence<Is...>)
    : proposed{ {static_cast<void>(Is), -1}... }
    {}
};
