# Spin Locks and Backoffs Exercises

## 85:

If we remove the following line from `unlock()` in our `CLH` lock:

```
myNode.set(myPred);
```

What can do wrong? 

The problem is that, when we enter the `lock()` method, we assume there are no dependencies on our personal node. If we do not change the `myNode` value to something we know not to be in use (the predecessor, that we know we are the only depencency for),
we can get a situation like follows. We call `unlock()`. Before the thread waiting on us realizes, we call `lock()` again, set our locked state to true, and now the other thread, that it __still__ depending on us, will wait forever.

## 86:

Say we want to implement a barrier that will wait until `n` threads have accomplished some condition. Evaluate the two following suggestions:

1. We have a shared counter (that is protected by a lock for some reason). Every thread updates it, and then spins until it equals `n`
2. We have an `n`-element `boolean` array. Thread `0` sets `b[0]` to `true`, thread `i` waits until the `i-1 th` element is true and then sets the `i`th element to true and continues


The first is terrible: so much contention and shared memory for this one lock, and we keep coming back to it. Agh!

Second one... doesn't work? If threads `0,1` both finish but there is also a `thread2` that has not finished, thread1 will hapilly proceed.

Proposal for the second one:

Now we have two boolean arrays. In the first one, `thread0` sets `a[0]` to true, and thread `i` waits until the `i-1`th element is true before setting the `ith` element to true.

In the second array, `threadn` sets `b[n]` to true, and thread `i` waits until the `i-1`th element is true before setting the `ith` element to true.

And then we now we can leave when, if we are thread `i`, `a[i]` and `b[i]` are both true.

## 87:

Prove that the CompositeFastPath lock provides mutual exclusion but is not starvation free:

_Mutual Exclusion_: Taking the fast path is only possible when A) no other thread is on the fast path, but more importantly when the `tail` is null. Clearly this is mutually exclusive with regular use of the lock.

_Not Starvation Free_: Here is the starvation case. Say thread1 holds the lock on the fast path. Thread2 joins, acquires the base lock, and is in the while loop waiting for the fast path to be given up. No guarantee that we release this, and so we could totally starve thread2 like this.

## 88:

Note that we can have a case of a node being the tail of both its local queue and the global queue BEFORE its `tailWhenSpliced` flag is set. Explain why this is not an issue.

## 89:

Explain the issue that can occur when the time between becoming the cluster master and successfully splicing onto the global queue becomes too small. Suggest a remedy.

## 90:




## 91:

Design an `isLocked()` method that tests if a thread is holding a lock. Give implementations for 

1. Any `testAndSet()` spin lock
2. The `CLH` queue lock
3. The `MCS` queue lock

```cpp
struct TAS
{
    TAS() : locked(FREE)
    {}
    void lock()
    {
        while(locked.load() != FREE)
        {
            if(locked.compare_exchange_strong(FREE, std::this_thread::get_id()))
            {
                return;
            }
        }
    }
    void unlock()
    {
        locked.set(FREE);
    }
    bool isLocked() const
    {
        return locked.load() == std::this_thread::get_id();
    }
private:
    static constexpr int FREE = -1;
    std::atomic<size_t> locked;
};
```

```cpp
struct Qnode
{
    Qnode() : locked(false) {}
    std::atomic<bool> locked;
};
struct CLHLock
{
    std::atomic<Qnode> tail;
    thread_local Qnode MyNode;
    thread_local Qnode MyPred;

    void lock()
    {
        Qnode tailLocal = tail.load();
        Qnode localMe = myNode.load();
        localMe.locked.set(true);

        while(!tail.compare_exchange_strong(tailLocal, localMe));

        myPred.set(tailLocal);

        while(myPred.locked);
    }

    void unlock()
    {
        Qnode localMe = myNode.load():
        QNode pred = myPred.load();

        localMe.locked.set(false);
        myNode.set(pred);
    }

    bool isLocked() const
    {
        Qnode localMe = myNode.load();
        Qnode pred = myPred.load();
        return localMe.locked.load() && !pred.locked.load();
    }
}
```

```cpp
struct Qnode
{
    Qnode() : locked(false), next(nullptr)
    {}
    std::atomic<bool> locked;
    Qnode* next;
};
struct MCSLock
{
    thread_local Qnode myNode;
    std::atomic<Qnode> tail;

    void lock()
    {
        Qnode node = myNode.load();

        Qnode localTail = tail.load();

        while(tail.compare_exchange_strong(localTail, node));
        
        if(localTail != nullptr)
        {
            node.locked = true;
            localTail.next = node;
            while(node.locked);
        }
    }

    void unlock()
    {
        Qnode node = myNode.load():

        if(!node.next)
        {
            if(tail.compare_exchange_strong(node, nullptr))
            {
                return;
            }

            while(!node.next);
        }

        node.next.locked.set(false)
        node.next = nullptr;
    }
    
    void isLocked()
    {
        Qnode localTail = tail.load();
        Qnode node = myNode.load();
        bool localLocked = node.locked.get();

        if(localLocked == false && node.next && node.next.locked == true)
        {
            return true;
        }
        else if(localLocked == false && localTail == node)
        {
            return true;
        }
        else
        {
            return false;
        }

    }
};
```
