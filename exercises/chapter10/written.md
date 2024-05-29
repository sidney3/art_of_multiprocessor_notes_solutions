# Chapter 10 Exercises:

## 119:

Change the queue implementation to work with null items. As we use `nullptr` to signify that an item does not exist, we will just switch to using `std::optional`

## 120:

Consider the following lock-free queue implementation:

```cpp
template<typename T, long N>
struct SRSWQueue
{
    void enq(T item)
    {
        while(tail.load() - head.load() == N);
        
        _data[tail.load() % N] = item;

        // need to make sure that this write doesn't get re-ordered until after the increment!
        // otherwise data race and also we're showing data that we shouldn't

        tail.get_and_increment(std::memory_order_release);

    }
    T deq(void)
    {
        while(tail.load() == head.load());

        T res = _data[head.load()];

        // likewise to above

        head.get_and_increment(std::memory_order_release);
        return res;
    }
private:
    std::atomic<int> head;
    std::atomic<int> tail;
    std::array<T, N> _data
    
};
```

## 122.
Consider the lock based bounded queue implementation and its `deq()` method. Recall:

```cpp
template<typename T>
std::unique_ptr<T> Queue<T>::deq(void)
{
    bool wake_enqueuers = false;
    std::unique_lock<std::mutex> lk{ deqLock };

    notEmpty.wait(lk, [this](){return size.load() > 0;});

    auto res = std::make_unique<T>(head->value);
    head = head->next;

    size_t old_size = size.get_and_subtract(1);

    if(old_size == capacity)
    {
        notFull.notify_all();
    }
}
```

Why do we first lock before checking if we are empty? Is this necessary? Yes: when we release from the cv we need to be the only thread that does so, and the lock accomplishes this.

## 123.

We have 5 people around a pot of stew. Each one holds a spoon that reaches the stew but where the handle is much longer than each ones own arm. Can you devise a strategy that allows them to feed each other? Two people cannot feed the same person at once.

Here is an inefficient solution: we number the people 1,...,5. We have a "variable" (number that everybody agrees on) that starts at 1. When a variable is at a given value n, it is that person's turn to be fed. Let `m = (n+1)%5`. Person `m` asks person `n` if they are hungry. If they are they feed them, and then we set our variables value to `n`.

Maximally bad solution: only one person is fed at a time and in order for one person to be fed potentially every single person needs to be asked if they are hungry.

Here is a more concurrent approach. So everybody keeps a counter of the number of other people that have gotten to eat SINCE THEY LAST ate (every time somebody eats, you take note of that, and when you eat you reset your counter to zero). And everybody is constantly repeating their own number out loud (if they are not in the process of eating). 

So everybody who wants to eat puts their hands out. If evry person puts their hands out, they all say their numbers and whomever has the lowest number puts their hands back in. Then there are feeders. A feeder will first put his spoon on a persons hands to mark they are about to feed them. If two feeders put their spoon at the same time, then the one with the lower index will put his spoon back. Then, the person whos hands have been marked puts their hands back out, and is fed. Repeat.

This is a bit better: more than one person can get fed at a single time. But we manage a lot more state. Every thread needs to be keeping track of every action of every other thread, and is constantly transmitting their state to every single other thread (though I suppose you could just tune out the other threads). Also note that in a scenario where few people are hungry, this is great.

But here is a better way to decide conflict: random backoff. Everybody who is hungry puts their hands in the middle. If they wait a certain amount of time without being fed, they retract their hands for a random amount of time. Any feeder will randomly choose one of the hands to feed, and will mark that they are about to feed them by putting the spoon on their hand. Same protocol for breaking feeding ties as before.

## 124:

Consider the lock free linked list based queue from before. Recall that these are the `enq` and `deq` methods.

```cpp
template<typename T>
struct Node
{
    template<typename ... Args>
    Node(Args&&... args) :
    value(std::forward<Args>(args)), next(nullptr)
    {}
    T value;
    std::atomic<Node<T>*> next;
};

template<typename T, typename ... Args>
void Queue<T>::enq(Args&&... args)
{
    auto node = new Node<T>{std::forward<Args>(args)...};
    while(true)
    {
        auto localTail = tail.load(std::memory_order_acquire);
        auto next = localTail->next;

        if(localTail != tail.load())
        {
            continue;
        }

        if(next == nullptr)
        {
            if(localTail->next.compare_exchange_strong(next, node))
            {
                tail.compare_exchange_strong(localHead, node);
                return;
            }
        }
        else
        {
            tail.compare_exchange_strong(localTail, next);
        }
    }
}

template<typename T>
std::optional<T> Queue::deq()
{
    while(true)
    {
        auto first = head.load();
        auto last = tail.load();
        auto second = first->next.load():

        if(first != head.load())
        {
            continue;
        }

        if(first == last)
        {
            if(second == nullptr)    
            {
                return std::nullopt;
            }
            else
            {
                tail.compare_exchange_strong(last, second);
            }
        }
        else if(head.compare_exchange_strong(first, second))
        {
            T res = first->value;
            reclaim(first);
            return res;
        }
    }
}
```

1. Can we choose the point at which the returned value is read from the node as the linearization point of `deq`?

Nah - it is once we successfully execute the CAS `tail.compare_exchange_strong(last, second)`. After this point the state (that we have dequeud this element) is observable. If we make the linearization point the assignment to the value, then another thread could observe the state BEFOR this happens and see that a deque has indeed arisen (as the head pointer has moved up).

2. Can we choose the linearization point of the `enq` method to be the point at which `tail` is updated, possibly by other threads?

Sure: definitely not going to observe the value before this happens, and once this happens we can always observe the value.

## 125: 

Consider the following unbounded queue implementation:

```cpp
template<typename T>
struct HWQueue
{
    std::array<std::atomic<std::optional<T>>, very_big_number> items;
    std::atomic<int> tail;

    template<typename ... Args>
    void enq(Args&&... args)
    {
        int i = tail.get_and_add(1, std::memory_order_acq_release);
        new(&items[i]) std::optional<T> (std::forward<Args>(args)...);
    }

    T deq()
    {
        while(true)
        {
            int upper_bound = tail.load(std::memory_order_acquire);
            for(size_t i = 0; i < upper_bound; ++i)
            {
                // doesn't exist in C++, but just roll with it
                auto found = items[i].load_and_set(std::nullopt);

                if(found.has_value())
                {
                    return *found;
                }
            }
        }
    }
};
```

Linearization point for enq will be when we set the value to non-null.

Linearization point for deq will be when we find non-null value and execute the `load_and_set`


