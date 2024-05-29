#include <atomic>
#include <format>
#include "../../exercises/chapter10/test_pool.h"

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

int int_fetch_add(std::atomic<int>& x, const int oper)
{
    int localCopy = x.load(std::memory_order_acquire);

    while(!x.compare_exchange_weak(localCopy, localCopy + oper));

    return localCopy;
}
template<typename T>
struct dual_stack
{
    dual_stack(int capacity)
        :  capacity{capacity}, head{0}, items{static_cast<size_t>(capacity)}
    {
    }

    template<typename ... Args>
    bool enq(Args&&... args)
    {
        while(true)
        {
            int index = int_fetch_add(head, 1);

            if(index >= capacity)
            {
                std::cout << std::format("full: {}\n", index);
                return false;
            }
            else if(index > 1)
            {
                while(items[index].filled.load())
                {
                    std::cout << std::format("enq spinning on {}\n", index);
                }

                new (&items[index].item) T{std::forward<Args>(args)...};
                items[index].filled.store(true);
                std::cout << std::format("enq successful {}\n", index);
                return true;
            }
        }
    }
    std::optional<T> deq()
    {
        /* std::cout << "deq initiated\n"; */
        if(head.load() <= 0)
        {
            return std::nullopt;
        }

        while(true)
        {
            int index = int_fetch_add(head, -1);

            if(index <= 0)
            {
                /* std::cout << std::format("deq failed: empty: {}\n", index); */
                return std::nullopt;
            }
            else if(index <= capacity)
            {
                while(!items[index].filled.load())
                {
                    /* std::cout << std::format("deq spinning on index {}\n", index - 1); */
                }

                T res = std::move(items[index].item);
                items[index - 1].item.~T();
                items[index - 1].filled.store(false);
                /* std::cout << std::format("deq successful of {}\n", index - 1); */
                return res;
            }
        }
    }
    bool empty()
    {
        return head.load(std::memory_order_acquire) <= 0;
    }
private:
    struct node
    {
        node() : item{stack_traits<T>::empty_value()}, filled{false}
        {}
        T item;
        std::atomic<bool> filled;
    };

    const int capacity;
    std::atomic<int> head;
    std::vector<node> items;
};

int main()
{
    dual_stack<int> S(1000);
    test_pool(S);
}

