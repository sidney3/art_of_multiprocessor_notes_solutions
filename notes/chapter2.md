# Mutual Exclusion

## Time

Reasoning about concurrent computation is mostly reasoning about time. Therefore we want more precise ways to refer to time and the state of threads.

We call a thread a `state machine` and its transition states are called `events`. Events are instantaneous -> it is convenient to require that events are never simultaneous. And a thread produces a sequence of events `a_1,a_2,...`. We say that the `jth` occurrance of event `i` is event `a_i^j`. An interval `(a_0, a_1)` is thus the duration of time between events `a_0` and `a_1` occurring. 

Similarly, we denote the `jth` occurrance of the interval `I_a` as `I_a^j`

We call a critical section of code a section that can only be executed by one thread at a given point in time. Furthermore, a thread is `well formed` if 
1. Each critical section is associated with a unique lock
2. The thread calls the lock's `lock` method when entering the critical section
3. The thread calls the lock's `unlock` method when leavint the critical section

We can now formalize the notions that we had before that a good lock algorithm should satisfy...

**Mutual Exclusion**: Critical sections of different threads do not overlap.

**Freedom From Deadlock**: If thread A calls `lock()` but never acquires the lock, it must be the case that the other threads are completing an infinite number of critical sections.

**Freedom From Starvation**: Every call to `lock()` eventually returns.

## 2 Thread Solutions

We will use the notation `write_A(x=v)` to describe an event in which thread `A` assigns the value `x` to `v`. And an event `read_A(x==v)` an event where thread `A` reads from `x` and checks if it is equal to `v`.

Consider the following locking algorithm:

```cpp
struct LockOne
{
public:
    LockOne() : flag()
    {}
    void lock()
    {
        volatile int i = std::this_thread::get_id();
        volatile int j = i - 1;
        flag[i] = true;
        while(flag[j]);
    }

    public void unlock()
    {
        volatile int i = std::this_thread::get_id();
        flag[i] = false;
    }
private:
    std::array<bool, 2> flag;

};
```

Lemma: this satisfies mutual exclusion. Suppose not. Thus, there intersecting critical sections between threads relying on this lock.

Let's look at the assignments of each thread before entering the critical section:

```
write_A(flag_A = true) -> read_A(flag_B == false) -> CS_A
write_B(flag_B = true) -> read_B(flag_A == false) -> CS_B
```

We therefore have that

```
read_A(flag_B == false) -> write_B(flag_B = true)
```

or else we would not have both of the above be able to pass. However, this implies that we also have

```
write_A(flag_A = true) -> read_B(flag_A == false)
```

A contradiction! 

This implementation has an issue, this being that it will deadlock if locks are interleaved. So let's consider another implementation

```cpp
struct LockTwo
{
    void lock()
    {
        int i = std::this_thread::get_id();
        victim = i; // let the other go first
        while(victim == i); // wait
    }
    void unlock()
    {}
private:
    volatile int victim;
};
```

Fascinating. We must wait until we get another thread that joins and locks, and then we are freed.

Again, proof that we can't have two overlapping critical sections:

```
write_A(victim = A) -> read_A(victim == B) -> CS_A
write_B(victim = B) -> read_B(victim == A) -> CS_B
```

ThreadB must assign `victim` to `B` between `write_a(victim = A)` and `read_A(victim == B)`, but this means that whenever we execute `read_B(victim == A)`, it will read that the victim is `B`.

Note that these two designs complement each other. One succeeds only when the two threads run concurrently, and the other succeeds only when they run separately.

## The Peterson Lock

```cpp
struct Peterson
{
    void lock()
    {
        int i = std::this_thread::get_id();
        int j = 1 - i;
        flag[i] = true; // I'm interested
        victim = i; // You go first

        while(flag[j] && victim == i);
    }
    void unlock()
    {
        int i = std::this_thread::get_id();
        flag[i] = false;
    }
private:
    volatile std::array<bool, 2> flag;
    volatile bool victim;
};
```

Again assume lack of mutual exclusion

Here is the sequencing of state changes for Peterson:
```
write_A(flagA = true) -> write_A(victim = A) -> 
    read_A(flagB == false) -> read_A(victim == B) -> CS_A

write_B(flagB = true) -> write_B(victim = B) ->
    read_B(flagA == false) -> read_B(victim == A) -> CS_B
```

For these to pass, we thus need that

```
write_A(victim = A) -> write_B(victim = B) -> read_A(victom == B)
(1)
```
OR
```
read_B(flagB = false) -> write_A(flagB = true)
(2)
```

AND
```
write_B(victim = B) -> write_A(victim = A) -> read_B(victim == A)
(3)
```
OR
```
read_A(flagB == false) -> write_B(flagB = true)
(4)
```

if `(1)` is true, then neither of `(3)` or `(4)` can be true. And similarly, if `(2)` is true it messes up the sequencing for `(3)` and `(4)`

Basically, the key observation is that both of the read/write pairs depend on the other to happen first.


Note that we CANNOT change the ordering of the flag writing! Look
what happens when we do...

```
// BAD PETERSON

write_A(victim = A) -> write_A(flagA = true) -> 
    read_A(victim == B) -> read_A(flagB == false) -> CS_A

write_B(victim = B) -> write_B(flagB = true) -> 
    read_B(victim == A) -> read_B(flagA == false) -> CS_B
```

Then we can get the following sequence. Basically the observation is that setting the flag needs to be done before setting the victim.

```
write_A(victim = A) -> write_B(victim = B) -> write_B(flagB = true)
    -> read_B(victim == A) (F) -> read_B(flagA == false) (T) 
    -> write_A(flagA = true) -> read_A(victim == B) (T)
    -> read_A(flagB = false) (F)
```

The critical relation (and guarantee) that the victim gives us is that we will always have the relationship (WLOG, clearly neither can pass with only flags)

```
write_A(victim = A) -> write_B(victim = B) -> read_A(victim == B)
```

So if `B` is to pass it will do so on flags, so `B` will need that

```
read_B(flagA == false) -> write_A(flagA = true)
```

And so if we enforce that writing to the flag always comes before writing to the victim, then we would need that

```
write_A(flagA = true) -> write_A(victim = A) -> write_B(victim = B)
    read_A(victim == B)
```

And thus, from the first relation we would have that

```
read_B(flagA == false) -> the other stuff, that includes a write by B
```

But the critical observation is that if we need the following to be true:

```
write_A(victim = A) -> write_B(victim = B) -> read_A(victim == B)
```

Then if we place our flag writes BEFORE our victim writes, it means that ALL writes have to be complete before any reads are complete, and this is a strong guarantee.




More interesting is the guarantee of deadlock.

Here is the proof given by the book, that is quite elegant:

Suppose not. Then suppose WLOG that A runs forever in the lock method. Then, A *must* be running in the while loop. So what is B doing? Well, it must not be advancing either - otherwise it would set victim, and it can ONLY set it to `B`, in which case we break out of the loop.

Therefore both A and B are stuck. This however means that victim is equal to both `A` and `B`. So clean.

We now present some mutual exclusion protocols that work for protocols with `n` thhreads
## The Filter Lock

Direct generalization of the Peterson lock to `n` threads

```cpp
template<long N>
class Filter
{
    Filter(int n) : level(), victim()
    {}

    void lock()
    {
        int me = std::this_thread::get_id();
        for(int i = 1; i < N; i++)
        {
            level[me] = i; // level[i] indicates the level that our thread
            // is trying to enter

            victim[i] = me;

            // while there exist conflicts, spin
            bool conflicts_exist = true;
            while(conflicts_exist)
            {
                conflicts_exist = false;

                for(int other = 0; other < N && !conflicts_exist; other++)
                {
                    if(other == me)
                        continue;

                    conflicts_exist = conflicts_exist || 
                        (level[other] >= i && victim[i] == me);
                }
            }
        }
    }

    void unlock()
    {
        int me = std::this_thread::get_id():
        level[me] = 0;
    }
private:
    volatile std::array<int, N> level;
    volatile std::array<int, N> victim;
};
```

At most `n` threads that enter level 0, at most `n-1` threads that enter level 1,..., at most 1 thread that enters the critical section (past the last level).

Each level has at least one victim thread that will never be allowed to pass as long as there are other threads that are at that level or higher.

And great, we can modify the victim flag, but we will modify it to ourself.

So the we present a proof that if there are `k` threads that enter a given level, it will be impossible for all `k` of them to pass through it (presuming none enter the critical section and unlock).

`victim` will have to be one of them at any given point in time (we assume that there are no more than `k` that come in), and each must set the victim to themself before passing through. 

Do we have a proof that at least one of will get through? Sure - all but the last one will get through, always. 

## Fairness

That said, we have no way of telling if the above code is fair (that is, in this context:)

Let's first define some notions.

A doorway section is a section completed in the `lock()` method that we know will finish in a finite number of steps. A waiting section in the lock method is a section completed once the doorway section is done and one that can take an infinite amount of time.

A lock is first come first serve if, if a thread finishes its doorway before another thread, then it will enter the critical section before that thread.

## The bakery algorithm

The idea behind this algorithm is clean! In the doorway phase, each thread takes a unique identifier, and then waits until no thread with an earlier number is trying to enter before it. My thought would be that we don't even need to use some weird boolean to keep track of if a thread is waiting or not. Just, once we call the `unlock()` method, we set our value to some infinity like type.

Or actually we just use a flag variable. Note that we use lexicographical ordering -> tiebreak with index of threads. I don't need to write the code for this it is straightforward enough.

## Timestamps

But this raises issues about if we will overflow. Really, what we want is a way to get time stamps in a wait free way. Furthermore, if one thread takes the label after another, then it should have a larger label. Thus, a thread needs to have two abilities:

A. read other threads timestamps (scan)
B. assign itself to a later timestamp (label)

We can do this with a real timestamp system, but we also present an interesting solution: a directed graph satisfying the following properties:

1. irreflexive: no edge to itself
2. antisymmetric: if we have an edge from a->b, then we don't have an edge from b->a

The thread performs the scan by looking at all the nodes from other threads currently waiting, and then gives itself a node that has an edge from all those nodes to our new node. Unsurprisingly, such a construction leads to an infinite graph. So what if we tried to be smarter. Instead, we use a precedence graph `T2` that is just a cycle of three nodes, with the precedence that `0 < 1, 1 < 2, 2 < 0` (note that this is only in a world with two threads)

The order will thus never be ambigious, and we can continue to use our scanning method. That said, we can run into some issues with more than two threads...

Why could C4 not work for 3 threads? If we choose `1` and `3` for the first two nodes, we want to be able to choose a value that is greater than both of these. But our only choices are `0` and `2`! No good. Let `G` be a precedence graph, and `A,B` subsets of `G`. We say that `A` dominates `B` if every node of `A` has edges directed to every node of `B`. Then let Graph multiplication be the following operator:

`G * H` = replace every node of `v` of `G` with a copy of `H`, which we denote `H_v`, and let `H_v` dominate `H_u` in `G * H` if `v` dominates `u` in `G`.

Then we inductively define `T_k` as follows:

1. `T_1` is a single node
2. `T_2` is the precedence tree from before
3. for `k > 2`, `T_k = T_2 * T_{k-1}`

And we aren't saying that we can never have cycles, but that this gives us an algorithm for preventing them. Look at the case for three nodes: if we have two nodes that are in the same `T_2`, then we need just pick another node that is in the group that dominates them. If they are in two different `T_2` nodes, then we can just pick the one with the higher precedence and place our node in the spot such that it is of higher precedence that the active node in there.

Inductively, we see that if we have `k` of our `T_{k+1}` nodes placed, we will always be able to place another. Let our hypothesis be that this statemnt is true for `T_k` -> if we have less than `k` nodes in our graph that do not form a cycle, then we can always find a spot to place our next node that will not lead to a cycle.

Base case: `T_2`. See above reasoning.

Inductive case: 
    Case 1: all of our nodes are in a single `T_2` cell -> and thus we can just place our new node on the single other node that dominates this node.
    Case 2: not all of our nodes are in a single `T_2` cell block. Thus we have at least one cell block that has less than `k - 1` nodes. Thus we can place our node into this cell without creating any inner cycles. Note that we must not have all three cell blocks filled, so this will never result in a cycle outside of the block.

## Lower Bounds on the Number of Locations

The issue with the above algorithm is that is requires all threads to read and write from the same `n` memory locations, where `n` is potentially large. This is about the worst thing you can do on a computer, so we would like to avoid it. But we claim that with only memory reads and writes, this is a lower bound.

Def: the **state** of an object is the state of its fields. A thread's **local state** is the state of all its objects and its local variables. And the **global state** is the state of all objects plus the local state of all threads.

Def: A `Lock` object state `s` is **inconsistent** if we are in a state where some thread is in the critical section but the lock is consistent with a global state in which no thread is in a critical section or trying to enter a critical section.

Lemma: no deadlock free `Lock` algorithm can enter an inconsistent state:
Proof: 
    When some thread `B` tries to enter the critical section, we know that it must eventually succeed. To do so it must reason about if there is currently a thread in the critical section, and this offers a contraction because, as the lock is in an inconsistent state, we have no way to reason about this.

Def: a **covering state** for a lock object is a state in which there is at least one thread about to write to each shared location, but the object's locations appear as if the critical section is empty (consistent with a global state in which this is the case). In such a state, we say that a thread covers the location that it is about to write to. An example of this would be the `victim` object in the first `TwoLock` algorithm

**Theorem**: any lock algorithm that, by reading and writing memory, solves deadlock free mutual exclusion for three threads, must use at least three distinct memory locations.

Proof: assume a deadlock free algorithm for three threads with only two locations. Note that if we run a thread by itself and it enters a critical section, it must write to at least one location in memory, or else the state would be consistent with one in which no thread was in the critical section. 

If each thread writes to a distinct state before entering the critical section, we are done. So let's assume that this is not the case (i.e. we have shared locations). Let's first imagine a scenario, prove it leads to a contradiction, and then prove that it is possible.

Assume we have only two distinct memory locations, and they are both covered by threads `A` and `B` respectively. Then thread `C` enters. Certainly, as the state is consistent with a state where no thread is waiting, `C` should enter the critical section eventually. And it can mark whatever state it wants - all of our allowable state will be overwritten by our two covering threads `A` and `B`! And thus our thread is an inconsistent state - because we are in a state compatible with no threads in the critical section and yet `C` is in the critical section!

So now we need to prove that we can maneuver threads `A` and `B` to such a situation. So let's denote our two locations in memory that we can write to `L_A`, `L_B`. So `B` enters the critical section and is poised to write to `L_B`. Then, thread `A` enters and is poised to write to `L_A`. Note that we are not in a covering state, as `A` could have written `L_B` to indicate to thread `C` that something is up (that `B` is waiting). So then assume that thread `B` runs through the critical section at most 3 times (thus setting the state back to one in which no thread is waiting). And so then we are back in a situation where `B` covers `L_B`, and we get the desired situation.

The underlying logic at play is as follows: after a thread writes something, another thread can overwrite it again without anybody ever knowing that the first write happened!
