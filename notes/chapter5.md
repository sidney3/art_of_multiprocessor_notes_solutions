# Relative Power of Synchronozation Primitives 

## Consensus Protocols

A problem that, at first glance, seems kinda random, but is very useful as a metric for evaluating the relative strength of a synchronization primitive:

The problem is as follows: we say that a solution to the consensus problem for `n` threads is an object `consensus` with a method `decide(T)` that satisfies the following contraints:

1. For at least one thread, the result of `decide(T t) == t`
2. The result of the function is always the same regardless of input.

The consensus number for a synchronization primitive `S` is the greatest `n` such that there exists such a wait free implementation.

For simplicity reasons we do the proofs with the binary decision problem, but same results apply. Note a critical state is a state in which any step from either thread will
We say our state is __bivalent__ if there are children in its outcome subtree for both outcomes (0 and 1) for `decide`. We say a state is __univalent__ if... you can guess.

We call a critical section a bivalent section from which any step from any thread decides the outcome immediately -> as we are wait free it is always possible to drive two threads to such a state.

**Theorem**: the consensus number of atomic registers is 1. 

Let's take a scenario with two threads, drive them to a critical section, and then continue. Both threads must be about to execute a write, otherwise, if one executes a read and the other continues solo, it will be indistinguishable from a state in which that other thread moved first.

Similarly, if both threads write to different locations, then their actions commute so the two possible states after they each take their first move are indistinguisable.

Finally, if both writes are to the same location, then the thread that acts second will trample the write of the first.


Note that we also have a relatively trivial solution to consensus n = 2 for a MRMWFIFO queue -> our startegy is that we try and deduce who made the first write -> so jsust make a queue with back element "WINNER", front element "LOSER" and have a thread pop from this queue to decide who goes first.

## Multi Assignment Objects:

A multi assignment object `n >= m > 1` is an object with a method `assign()` that takes a list of `m` indices and `m` values and atomically sets each index to each stated value in a given object with `n` fields (often an array).

For `n = 3, m = 2` we have concensus number `>= 2`: just make a shared array of `3` elements, have the first thread write to the first two elements, with the write to the first element being the actual write, and the write to the middle element being a flag for `0`, and the second thread do the same for the last two elements. Then we can again always figure out who went first...

We also have that the conensus number for `n, (n choose 2)` is `n` -> we make one array of length `n` that we will store the writes for each thread to, and then a second array an array of `n choose 2` elements where each element is assigned a distinct pair of elements in the array,  and so we overwrite each of our elements with a multi assignment object when it is our turn, and this gives us a total ordering on the order of the calls, because we can "sort" the elements (for each two element pair we know which came first because it will be the one with the "bottom" write)

## RMW Operations

Read Modify Write operations: given a set of functions `F` we take an element, apply one of the functions `f_i` to the object, and return its original value (before applying the function)

Any nontrivial (not only identity) RMW has a consensus number of >= 2 -> we can perform the `RMW` operation on a single shared value `v`, and if the return is `v` we know we are the winning thread.

We say an `RMW` register is a common2 register if it satisfies one of the following conditions:

1. Each pair of functions commutes (think `getAndAddOne()`)
2. One function always overwrites the other (i.e. `f_i(f_j(v)) = f_i(v)`) (think `getAndSet()`)

### Theorem: a common2 register has consensus number 2

The gist of the reasoning is that the winner can tell they have won, but the two losers (who know they are losers) don't know who the winner is

At a critical state with threads `A,B,C`, as before must be modifying a shared register, and as atomic operations only have consensus number 2 so we are definitely going to be making a call to `getAndSet()`

Case 1: Each function tramples the other-> so we it doesn't matter if `A` or `B` makes their first step first, `f_i(v)` = `f_j(f_i(v))` and so the state externally will be indistuinguidable to `C` despite each carrying a distinct outcome
Case 2: Again, state is induistinguishable

## CAS:

the king of the jungle:

```cpp
template<typename T, long N>
struct consensus
{ 
    std::atomic<int> r;
    static constexpr int WIN = -1;

    consensus() : winning_thread(WIN)
    {}
    T decide(T t, size_t thread_id)
    {
        propose(t, thread_id);
        if(CAS(&r, WIN, thread_id))
        {
            return t; 
        }
        else
        {
            return proposed[r.get()];
        }
    }
};
```

