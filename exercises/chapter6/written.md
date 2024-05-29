# Universality of Consensus Exercises

## Give an example showing how a universal construction can fail for object with non-deterministic sequential specifications

Obviously each time we make an update we replay all the state updates, and this is not always consistent...

## Give a solution:

Save the result of the state update with a consensus object!

## 79: Would cease to work for the wait free construction:


Would break the second as we would choose the sentinel object for our new object to add, and it doesn't have a well defined Invoc method...

## 80: If our protocol is to go help yourself first and then the helper thread you have been assigned to, will this still be wait free?

No - we could just always succeed and thus starve a thread...

## 81: 

Q: we use a "distributed" implementation of `head` to "the node whose `DecideNext` field it will try to modify" to avoid having an object that allows repeated consensus. Replace this implementation with one that has no `head` reference and instead finds the next "head" by traversing down the log from the start until it reaches a node with a sequence number of `0` or with the highest non-zero sequence...

What the fuck does "repeated consensus" mean? Oh - we want to avoid using the same `Consensus` object to make add the same node twice?

So it actually is a big issue if we were, say, to just pick a random head object! Let's say we pick an outdated head object - we get its next node, great that next node is what was proposed forever ago. But when we make the new node and set it, that new node does not have any of the next nodes! (With our pointer implementation we don't have this issue but ignore this).

## 82:

Necessary for the silly lemma, not the algo (behavior is unchanged)

## 83:

Propose a way to fix the universal construction to work with a bounded amount of memory. That is, a bounded number of consensus objects and a bounded number of read-write registers.

Idea: move the tail up dynamically as we work our way through the program!

Finite consensus objects??

Suggestion from the book: use before fields for the nodes and build a memory recycling scheme...

What we know: each thread will call decide at most once, so at most `N` calls to each decide object. 

Once a thread is no longer max, it is probably not gonna get chosen again. But the issue is that there totally could be a thread about to choose it...

So beautiful. Each Node has a reference count. When a node is "before" (meaning it is the node before the chosen new node, preferred, is has a boolean flag `destroy_me` set to true)

The next time it has a ref count of 0, it destroys itself!

```cpp
struct Node
{
    std::atomic<int> ref_count;
    std::atomic<bool> destroy_me;

    std::atomic<Node*> next;
    std::atomic<int> seq;
    Consensus<Node> decide_next;

    FIFO<Node> SharedResourceQueue;

    void increaseRefCount()
    {
        ref_count++;
    }
    void decreaseRefCount()
    {
        ref_count--;
        if(ref_count == 0 && destroy_me)
        {
            destroy_protocol();
        }
    }
    void destroy_protocol()
    {
        SharedResourceQueue.push_back(std::move(this));
    }
}


struct WFUniversal
{
    ...
    Result apply(Invoc invoc, size_t i)
    {
        while(announce[i].sequence == 0)
        {
            // ...
            if(after.sequence != announce[i].sequence())
            {
                after.before.decreaseRefCount();
            }

            // ALSO BEFORE EACH TIME WE ASSIGN A THREAD TO THE HEAD, 
            // WE INCREASE ITS REF COUNT
        }

        announce[i].before.destroy_me = true;
        after.before.decreaseRefCount();
    }
};
```

## 84: 

Implement a consensus object that is accessed more than once by each thread using `read()` and `compareAndSet()` methods, creating a "multiple access" consensus object. Do this without the Universal Construction.

Trivial:

```cpp
template<typename T, long N>
struct MultiConsensus
{
    std::array<T, N> set_value;
    Consensus<T> consensus;

    T decide(T value, size_t thread_id)
    {
        if(!set_value)
        {
            T res = consensus.decide(value);
            set_value[thread_id] = res;
        }
        return set_value[thread_id];
    }
};
```

Or alternatively

```cpp
template<typename T, long N>
struct multiConsensus
{
    std::atomic<int> decided_thread;
    std::array<T, N> proposed;
    static constexpr WINNER = -1;

    T decide(T value, size_t me)
    {
        if(!proposed[me])
        {
            propose[me];
        }

        if(decided_thread.CAS(WINNER, -1))
        {
            return propose[me];
        }
        else
        {
            return propose[decided_thread.read()];
        }
    }
};
```
