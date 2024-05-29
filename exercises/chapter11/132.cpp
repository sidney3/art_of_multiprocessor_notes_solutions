#include "../chapter8/97.cpp"
#include "../chapter10/test_pool.h"
#include <format>
#include <thread>



/*

   Just not working: the writer can get ahead of the reader and then its
   over - just spinning forever.
*/
int fetch_sub(std::atomic<int>& x, const int d)
{
    auto localCopy = x.load(std::memory_order_acquire);

    while(!x.compare_exchange_weak(localCopy, localCopy - d));

    return localCopy;

}
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
    stack(size_t capacity = 1024) : 
        capacity_{capacity}, 
        data_(capacity, stack_traits<T>::empty_value()),
        room_{}
    {}

    template<typename ... Args>
    bool enq(Args&&... args)
    {
        while(true)
        {
            if(head.load() == capacity_)
            {
                return false;
            }

            room_.enter(PUSH);

            bool oper_success = false;
            T to_add{std::forward<Args>(args)...};

            if(room_.exit([this, &to_add, &oper_success](){
                size_t top = head.load();
                if(top == capacity_)
                {
                    oper_success = false;
                    return;
                }
                oper_success = true;
                data_[top] = std::move(to_add);
                head.store(top + 1);
                        }))
            {
                if(oper_success)
                {
                    return true;
                }
            }
            else
            {
                std::this_thread::yield();
            }
        }

    }

    std::optional<T> deq()
    {
        while(true)
        {
            if(head.load() == 0)
            {
                return std::nullopt;
            }

            bool oper_success = false;
            room_.enter(POP);

            if(room_.exit([this, &oper_success](){
               size_t top = head.load();
               if(top == 0)
               {
                    oper_success = false;
                    return;
               }

               oper_success = true;
               popped_val = std::move(data_[top - 1]);
               head.store(top - 1);
               }))
            {
                if(oper_success)
                {
                    return popped_val;
                }
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }
    bool empty()
    {
        return head.load(std::memory_order_acquire) == 0;
    }
private:
    static constexpr size_t PUSH = 1;
    static constexpr size_t POP = 2;
    const size_t capacity_;
    std::vector<T> data_;
    std::atomic<size_t> head = 0;
    Rooms room_;
    static thread_local T popped_val;
};

template<>
thread_local int stack<int>::popped_val = stack_traits<int>::empty_value();

int main()
{
   stack<int> S{1024};
   test_pool(S);
}
