#include <vector>
#include <optional>
#include "../chapter10/test_pool.h"

template<typename T>
struct stack_traits
{
    static T empty_value();
};

template<>
struct stack_traits<int>
{
    static int empty_value()
    {
        return -1;
    }
};
template<typename T>
struct stack
{
    stack(size_t capacity)
        : m{}, capacity_{capacity}, data_(capacity, stack_traits<T>::empty_value())
    {}

    template<typename ... Args>
    bool enq(Args&&... args)
    {
        std::unique_lock<std::mutex> lk{m};

        if(head_ == capacity_)
        {
            return false;
        }

        data_[head_] = T{std::forward<Args>(args)...};
        head_++;
        return true;
    }

    std::optional<T> deq()
    {

        std::unique_lock<std::mutex> lk{m};

        if(head_ == 0)
        {
            return std::nullopt;
        }

        T res = data_[head_ - 1];
        head_--;
        return res;
    }
    bool empty()
    {
        return head_.load() == 0;
    }

    std::mutex m;
    std::atomic<size_t> head_ = 0;
    size_t capacity_;
    std::vector<T> data_;    
};

int main()
{
    stack<int> S(1024);
    test_pool(S);
}
