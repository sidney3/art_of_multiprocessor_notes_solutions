# Spin Locks and Contention

So the Peterson lock, while great, doesn't work on real systems, because reads and writes can be re-ordered. One option is to put in place memory fences, but these (allegedly) cost a similar amount to synchronization operations, so instead let's use these

## TestAndSet() locks

Humble test and set. This will set the value of a boolean to true and return its previous value (atomically).

So we can implement a spin lock using this as follows

```cpp
struct TASLock
{
    std::atomic<bool> state;
    void lock()
    {
        while(state.getAndSet(true));
    }
    void unlock()
    {
        state.set(true);
    }
};
```

```cpp
// test and test and lock
struct TTASLock
{
    std::atomic<bool> state;
    void lock()
    {
        while(true)
        {
            while(state.get());

            if(!state.getAndSet(true))
            {
                return;
            }
        }
    }
}
```

The latter performs faster, but is still very ineffecient... and scales terribly with the number of threads

 And think about the memory ineffeciency - each call to `state.getAndSet()` needs to reset the cache of everybody else, and so it broadcasts the address to the bus. The other processors snoop to the bus, and update their own caches.

What we really want is "local spinning" - threads reading their own cached values (that is, with a call like `state.get()`)


We want some way to get the thread to back off - the more unsuccessful retries it has, the more likely it should be to chill out and wait for some period of time. That logic is captured in the following class. Note that the performance of this can depend a lot on the constants chosen, so choose carefully...

```
struct Backoff
{
    const int min_delay, max_delay;
    std::atomic<int> limit;

    Backoff(int min_delay, int max_delay)
    : min_delay(min_delay), max_delay(max_delay), curr_delay(curr_delay)
    {}

    void backoff()
    {
        int delay = random(limit);
        limit = min(max_delay, 2 * limit);
        std::this_thread::sleep_for(delay);
    }
};
```

```
struct BackoffTTASLock
{
    static constexpr int min_delay = ...;
    static constexpr int max_delay = ...;
    std::atomic<bool> state;

    void lock()
    {
        Backoff backoff;
        while(true)
        {
            while(state.get());

            if(!state.getAndSet(true))
            {
                return;
            }
            else
            {
                backoff.backoff();
            }
        }
    }
    void unlock()
    {
        state.set(false);
    }
};
```

## Queue Based Locks

These are much preferred, largely as each thread will spin in its own little land, not touching a shared variable all the time, before finally getting notified! Much more __cache coherent__

Simplest possible such implementation is an array lock:

```cpp

template<long N>
struct ArrayLock
{
    std::array<bool, N> is_free;
    std::atomic<int> tail;
    thread_local<int> my_slot;

    ArrayLock() : is_free(false), tail(0)
    {
        is_free[0] = true;
    }

    void lock()
    {
        int slot = tail.getAndIncrement() % size;
        my_slot.set(slot);

        while(!slot);
    }
    void unlock()
    {
        is_free[my_slot] = false;
        is_free[my_slot + 1 % size] = true;
    }
};
```

That said, we will still get contention due to false sharing! SO really we should align our bools with the size of a cache line!
 
The problem, though, is space ineffeciency. Clearly, each lock will take `O(n)` space for each thread that we have. So what if we did it with a linked list? So clean...

```cpp
struct LockNode
{
    std::atomic<bool> locked;
};

struct CLHLock
{
    thread_local LockNode MyNode;
    thread_local LockNode MyPred;
    std::atomic<LockNode> tail;

    void lock()
    {
        MyNode.locked.set(true);
        MyPred = tail.getAndSet(MyNode); 
        // as I will run after tail runs, I put my own node on the list for after tail

        while(MyPred.locked.get());
    }

    void unlock()
    {
        MyNode.locked.set(false);
        myNode = MyPred; // ?? 
    }
};
```

Sn why do we need to be set to our predecessor? Note that WE are someone elses predecessor.

Ohhh, so we can't use our existing node because it is potentially referenced by other nodes. But there is nobody but me that references my predecessor!

Imagine if we leave `MyNode` as is - if we then call lock again before the thread that is waiting on us can unlock, we set our locked variable to false! That is bad news...

So we could also just make a new node, that might be wastful. So we set our current node to our predecessor node (which, recall, is the prior node's main node). We were the only dependency! Great waste free programming!!!

**THe book then cites the fact that we are waiting for the predecessors field to become false, not true! And this can cause performance to suffer in NUMA systems** Why???

No: the issue is that we don't control the place on which we spin: We are spinning on a location located in our parents thread...

```cpp
struct QNode
{
    std::atomic<bool> locked;
    std::atomic<QNode*> next;
}; // every QNode starts out unlocked and with a null next pointer

struct MCSLock
{
    std::atomic<QNode> tail; // initially nullptr
    thread_local QNode MyNode;
    void lock()
    {
        QNode qnode = MyNode.get();
        QNode pred = tail.getAndSet(qnode);
        if(pred != nullptr)
        {
            qnode.locked = true;
            pred.next = &qnode;
            // big difference:
            // we spin on our own variable!
            while(qnode.locked());
        }

    }

    void unlock()
    {
        QNode qnode = MyNode.get()l
        if(qnode.next == nullptr)
        {
            // if tail is still the QNode (i.e. the CAS line has not been executed, return)
            if(tail.compareAndSet(qnode, nullptr))
            {
                return;
            }

            // if the CAS has been executed, we wait for the assignment of the pred to happen.
            while(qnode.next == nullptr);
        }
        qnode.next.locked = false;
        qnode.next = nullptr;
        // this is so that, if we lock it again later on,
        // state is still consistent
    }
}
```

And now we come to the challenge of implementing a try_lock that will fail after a certain period of time, but that is queue based. This is trivial for any locking method where we aren't interacting with the other pieces (any spin locking method, that is), as we can just return. But for a queue, if we go dark and there is somebody waiting after us, and they want to wait for longer than us / they join at a much more opportune time than us, than we don't want them to be screwed.

A first solution works as follows:

we have a `.pred` variable each `qnode` that works as follows. `.pred == AVAILABLE` if it is free to go. `.pred == null` if occupied. Otherwise, it means that the node for which we are examining the `.pred` characteristic has died. Here is the impl

```cpp
struct TOLock
{
    static QNode AVAILABLE;
    thread_local<QNode> myNode;
    std::atomic<QNode> tail;

    void try_lock(time wait_for)
    {
        QNode newNode{};
        myNode = newNode;
        newNode.pre = null;
        QNode pred = tail.getAndSet(myNode);

        while(elapsed_time < wait_for)
        {
            QNode pred_of_pred = pred.pred;
            if(pred_of_pred == AVAILABLE)
            {
                return true;
            }
            else if(pred_of_pred != null)
            {
                pred = pred_of_pred; 
            }
        }
        if(!tail.compareAndSet(qnode, nullptr))
        {
            qnode.pred = pred;
        }
    }

    void unlock()
    {
        QNode node = MyNode.get();
        if(!tail.compareAndSet(node, nullptr))
        {
            node.pred = AVAILABLE;
        }
    }
};
```

/*
Note: the tail.compareAndSet(qnode, nullptr) don't add any functionality, just efficiency. If we just set the nodes predecessor to available, it will still work : a node will choose it as its tail, it will notice that it is available, and so it will break through!

Note also that we need to make a new node every time because an old node that we WERE using could still be in use somewhere in the chain!
*/

So now we proceed to a much more complicated lock. This is the `CompositeLock` which uses the following idea: in a queue lock, only the threads at the start of the queue do a lot of handoffs. So have a small number of threads waiting in a queue, and the rest trying to join that queue with exponential backoff.

The implementation will look as follows: The compositeLock class keeps an array of lock nodes. Each thread trying to join selects a node at random. If it is in use, backoff (exponentially). Once the thread actually enters the queue, it enqueues the node with a timeout style. It spins on the preceding node, and when the owner signals that the node is done, it enters the critical section. When unlocked, it releases ownership of the node and presumably indicates to any successor that it will have that it is done.

Note that we make the tail an atomic indexed reference to avoid the ABA problem (more on this to come!)

```cpp
public boolean tryLock(Time wait_for)
{
    Time start_time = current_time();
    std::optional<QNode> node = acquireQNode(wait_for, start_time);
    if(!qnode)
    {
        return false;
    }
    std::optional<Qnode> pred = spliceQNode(*node, wait_for, start_time);
    if(!pred)
    {
        return false;
    }

    return waitForPredecessor(*node, *pred, wait_for, start_time);
}
```

A QNOde has four possible states:

WAITING - linked into the queue, owner thread is in CS or waiting to enter
RELEASED - owner leaves CS and releases lock
Describe the states for when we abandon an attempt to use the lock
ABORTED - quitting thread has acquired node and enqueued it
FREE - quitting thread has acquired node but not enqueued it

So here is the code for `acquireQNode`. None of this for production. All of this written with maximum laziness...

```cpp
std::optional<QNode> acquireQNode(wait_for, start_time)
{
    Time end_time = start_time + wait_for;
    QNode chosen_node = nodes[randomely_chosen_index()];

    while(curr_time < end_time)
    {
        if(chosen_node.state.CAS(FREE, WAITING))
        {
            return chosen_node;
        }

        State node_state = chosen_node.state;
        QNode currTail = tail.get();

        /*

            Being the tail is a special case - it means that
            you have no successors that will come and
            update your state. So we accept this responsibility.
            
            Otherwise, such state management is handled in the waitForPredecessor() method
        */
        if((node_state == RELEASED || node_state == ABORTED)
            && node == currTail)
        {
            QNode prev = nullptr;

            if(node_state == ABORTED)
            {
                prev = node.prev;
            }

            // also done with timestamps to prevent ABA
            tail.CAS(currTail, prev);
        }

        backoff();
    }
}
```

```cpp
// standard enough stuff
std::optional<QNode> SpliceQNode(QNode node, Time expiry)
{
    QNode currTail;
    do
    {
        currTail = tail.get();
        if(now > expiry)
        {
            node.setState(FREE);
            return std::nullopt;
        }
    }(while tail.CAS(currTail, node))
}

bool waitForPredecessor(QNode pred, QNode node, Time expiry)
{
    State pred_state = pred.state;

    while(pred_state != RELEASED)
    {
        if(pred_state == ABORTED)
        {
            QNode tmp = pred;
            pred = tmp.pred;
            tmp.state = FREE;
        }
        if(out of time)
        {
            return false;
        }
        predState = pred.state;
    }

    pred.state.set(FREE);
    myNode.set(node);
    return true;
}
```

And the above is where most of the state management for the program happens.

Can't say I 100% understand everything but I definitely have the big picture

## Fast path version

The problem with this is the case where there is no contention: a thread must join, find a node, enqueue itself to that node, spin a cycle, before finally returning true. We want to augment this with a general fast path node that will operate more quickly in ideal circumstances (we have a node that we request to join with the fast path)

Note that this is a direct modification of the above algorithm - not so much a wrapper class as the OO fiends might hope.

That said, we use a very interesting way to signal that we have acquired the fastpath. Each tail object carries a timestamp, and so we just bitwise OR this timestamp with a higher order bit. Note that this therefore means that this bit is not accessible for the actual tail...

```cpp
struct CompositeFastPath : CompositeLock
{
    static constexpr int FASTPATH = ...;
    bool fastPathLock()
    {
        int oldStamp, newStamp;

        QNode node = tail.get(oldStamp);

        if(node != null)
        {
            return false;
        }

        if(oldStamp & FASTPATH)
        {
            return false;
        }

        newStamp = (oldStamp + 1) | FASTPATH;

        return tail.CAS(node, null, oldStamp, newStamp);
    }

    bool try_lock(Time time)
    {
        if(fastPathLock())
        {
            return true;
        }
        else if(this->try_lock(time))
        {
            while(tail.getStamp() & FASTPATH);

            return true;
        }
        return false;
    }
};
```

And then certainly unlocking is just clearing the FASTPATH mask...

## Hierarchical Locks

We assume that threads live in clusters - that is, other threads that they are local in memory to. And that communication to other local threads is cheaper than communication outside the cluster. Let's try to find a way to exploit this!

### Hierarchical TTAS Lock

Relatively simple: when the current thread holding the lock is in the same cluster as the current thread, we are more likely to propose to try and acquire the lock (as communication with this thread should be easier). Note that an issue this can cause is too much success / locality : all the threads in a given cluster starve those outside of the cluster!

This looks something like this

```cpp

struct HierarchicalTTAS
{
    static constexpr int FREE = -1;
    static constexpr int LOCAL_BACKOFF_MIN = ...;
    static constexpr int LOCAL_BACKOFF_MAX = ...;
    static constexpr int REMOTE_BACKOFF_MAX = ...;
    static constexpr int REMOTE_BACKOFF_MAX = ...;
    std::atomic<int> state = FREE;

    void lock()
    {
        int myCluster = std::this_thread::get_cluster();
        Backoff localBackoff(LOCAL_BACKOFF_MIN, LOCAL_BACKOFF_MAX);
        Backoff remoteBackoff(REMOTE_BACKOFF_MIN, REMOTE_BACKOFF_MAX);

        while(true)
        {
            if(state.CAS(FREE, myCluster))
            {
                return;
            }

            if(state == myCluster)
            {
               localBackoff.backoff();
            }
            else
            {
                remoteBackoff.backoff();
            }
        }
    }
    void unlock()
    {
        state.set(FREE);
    }
};

```

What does it mean to splice a queue to another queue?

It means that the front of our queue is waiting for the end of their queue, and if somebody goes to join their queue, they will wait for the end of our queue. The rest just works itself out!

So now we present the hierarchical queue. Quite tough to put down - has taken me a couple days. But the idea is as follows. Each cluster will keep its own local queue. Threads will join this local queue and this local queue will get added to a larger global queue, that resembles the CHL queue. Critically, this queue will now contain streaks of threads within the same cluster, and so we will promote cluster based locality.

Going back to the local queue, once we are on the local queue, we will wait until either:

A) the predecessor node is from the same cluster and tailWhenSpliced and successorMustWait are false
    Head of the global queue, so enters CS and returns
B) the predecessor node is from a different cluster OR the tailWhenSpliced flag is true
    the thread's node is at the head of the local queue.

Each Qnodes stores A) if the successor must wait, B) if it is the tail when spliced to the main queue and C) the cluster number. We do this with bits, but you don't need to focus on that. As far as we are concerned, the Qnode is a node with the atomic properties

```cpp
struct Qnode
{
    const int cluster_number;
    std::atomic<bool> successor_must_wait;
    std::atomic<bool> is_tail_when_spliced;
};

struct HCLHLock
{
    std::array<std::atomic<Qnode>, NUM_CLUSTERS> localQueues;
    std::atomic<Qnode> globalTail;
    thread_local Qnode currNode;
    thread_local Qnode predNode;
};

void HCHLock::lock()
{
    Qnode myNode = currNode.get();
    auto localQueue = localQueues[myNode.get_cluster()];

    QNode myPred = localQueue.get();
    while(!localQueue.compare_exchange_strong(myPred, myNode));

    if(myPred != null)
    {
        bool iOwnLock = myPred.waitForGrantOrMaster();
        // case 1: waitForGrantOrMaster delivers a grant: you have the lock
        // sir!
        if(iOwnLock)
        {
            predNode.set(myPred);
            return;
        }
    }

    Qnode localTail = nullptr;
    myPred = globalQueue.load();
    do
    {
        localTail = localQueue.get();
    } while(!globalQueue.compare_exchange_strong(myPred, localTail))

    localTail.setTailWhenSpliced(true);
    while(myPred.successor_must_wait());

    predNode.set(myPred);
    return;
}

void HCHLock::unlock()
{
    Qnode myNode = currNode.get();
    myNode.set_successor_must_wait(false);
    Qnode node = predNode.get();
    node.unlock(); // sets settings to safest possible - successor must wait and is not the spliced tail.
    currNode.set(node);
}

bool Qnode::waitForGrantOrMaster()
{
    
    while(true)
    {
        int localCopy = state.get();

        int bothFlags = tailWhenSpliced | successorMustWait;

        if(!(localCopy & bothFlags))
        {
            return true;
        }
        else if(localCopy & tailWhenSpliced)
        {
            return false;
        }
    }
}
```
