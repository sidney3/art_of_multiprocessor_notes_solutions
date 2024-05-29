# Chapter 11 Exercises

### 129:

Does it make sense to use the same backoff policy for `pushes` and `pops`. Answer: it depends on our algorithm. In the case that we are in `EliminationStack` land, then not really: if there are a bunch of `pushing` threads and then one `popping` thread arrives (even if it was once a `pushing` thread), then we __do__ want the pushing threads to backoff, and we __don't__ want the popping thread to do so as it is very likely to get an exchange hit!

However, in the case of our regular algorithm, both pushing and popping threads will be in contention with each other in the exact same way - the nature of the operation that they are doing is fundamentally the same: CAS with the same variable. And so the backoff policy should reflect this.

### 130

Implement a stack algorithm assuming that there is a bound at any given time between the total number of concurrent pushes and pops. 

Assume that this figure is `N`

This should make our exchanger more efficient, because it struggled most for cases where we had a lot of one type of thread, and we would rather this thread just be idly waiting than spamming an array.

So how can we improve our exchanger? 
1. Change the number of slots
2. Change our policy for assigning a range
3. Increase wait time

I feel like we can make our matching algorithm more efficient: still have an array, but keep variables `acquireIndex`, `deliverIndex` that work as follows:

```cpp
std::optional<T> pop()
{
    while(true)
    {
        if(is_empty())
        {
            return std::nullopt;
        }

        auto maybeValue = try_pop();

        if(maybeValue.has_value())
        {
            return *maybeValue;
        }

        size_t myAcquireIndex = acquireIndex.load_and_add(1);

        maybeValue = acquireArray.try(myAcquireIndex, wait_time);

        if(maybeValue.has_value())
        {
            return *maybeValue;
       }
        else
        {
            acquireIndex.load_and_subtract(1);
        }
    }
}
```

and presumably this value wraps around (defined behavior!) And this way we have a lot more coordination between the `acquirers` and the `givers`.

And in fact, applying this change to my own implementation (that has relatively few threads) yields great performance gains.

### 131

Consider the following stack based array implementation:

```cpp
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
    dual_stack(size_t capacity)
        :  capacity{capacity}, head{0}, items{capacity}
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
            else if(index >= 0)
            {
                new (&items[index].item) T{std::forward<Args>(args)...};
                items[index].filled.store(true);
                std::cout << std::format("enq successful {}\n", index);
                return true;
            }
        }
    }
    std::optional<T> deq()
    {
        std::cout << "deq initiated\n";
        while(true)
        {
            int index = int_fetch_add(head, -1);

            if(index < 0)
            {
                std::cout << std::format("deq failed: empty: {}\n", index);
                return std::nullopt;
            }
            else if(index < capacity)
            {
                while(!items[index].filled.load())
                {
                    std::cout << std::format("deq spinning on index {}\n", index);
                }

                T res = std::move(items[index].item);
                items[index].item.~T();
                items[index].filled.store(false);
                std::cout << "deq successful\n";
                return res;
            }
        }
    }
    bool empty()
    {
        return head.load(std::memory_order_acquire) == 0;
    }
private:
    struct node
    {
        node() : item{stack_traits<T>::empty_value()}, filled{false}
        {}
        T item;
        std::atomic<bool> filled;
    };

    const size_t capacity;
    std::atomic<int> head;
    std::vector<node> items;
};
```

Will it work? If not, is there something inherently wrong with the algorithm, or can it be remedied?

There is an off by one error in this code: the pop will take the top (unfilled) slot. But this ignores the bigger issue:

Multiple threads can be pushing / popping to the same slot, which will always lead to problems. Consider the following scenario:

`head` = 2

1. ThreadA: push. Pushing to 2, head = 3
2. ThreadB: pop: Popping from 2, head = 2
3. ThreadC: push: Pushing to 2, head = 3

And now we have two threads pushing to the same value. If we choose to order this insidiously, we can have that ThreadA and ThreadC both write their values and set the flag to true, and then thread B sets the flag to false, and now we have a slot with a false value below the head (i.e. the next time we try to pop from this we will spin forever). This is only one of the issues that we have: the fundamental issue is that the head variable can always get moved in both directions, and so the value that threads are writing to is not protected.

## 132

Explain why the following stack implementation doesn't work:

don't protect indices. With push->pop->push we have two threads trying to write to the same index.

Rewrite it using rooms


