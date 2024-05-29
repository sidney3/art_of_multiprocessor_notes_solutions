# Concurrent Objects

Behavior of concurrent objects is best described by their safety and "liveness" properties, known as *correctness* and *progress*. We examine different correctness definitions

For reference, we will consider the following implementation of a lock free FIFO queue with one reader and one writer.

```cpp
template<typename T>
struct SPSCQueue 
{
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    size_t _capacity;
    std::vector<T> items;

    SPSCQueue(size_t capacity) : 
    head(0), tail(0), _capacity(capacity), items(capacity)
    {}

    bool try_queue(T obj)
    {
        if(tail - head == capacity)
        {
            return false;
        }
        items[tail % capacity] = obj;
        tail++;
        return true;
    }

    std::optional<T> dequeue()
    {
        if(tail == head)
        {
            return std::nullopt;
        }

        return items[head % capacity];
        head++;
    }
};
```

## Quiescent Correctness

Definition: an object is quescent if it has no pending method calls. Therefore, the notion of quiescent correctness is that, any time an object is in a state of quescience, its state is equivalent to the state of some sequential execution of all method calls that have happened to it since its last period of quescience.

We can view this as follows:

calls separated by a period of quiescence will appear to take place in their completed order.

Remark that this is critically not necessarily the order in which these method calls were actually made!

Under what circumstances will quiescent correctness require a method call to block waiting for another to complete? Essentially never.

Def: a method is total if it is defined for every object state. Otherwise, it is partial. For example, consider an unbounded FIFO queue. The enqueue method is total, while the dequeue method is partial (as it is only defined for some states).

Def: a correctness property P is compositional if, whenever each object in a system satisfies P, the entire system satisfies this correctness property.

## Sequential Consistency

Def: We have sequential consistency when method calls appear to take place in program order (follow the order dictated by each thread) and meet the objects sequential specification.

Note that this is not necessarily a stronger condition than queiscent consistency - as we can reorder the methods in any order we like, assuming they respect the order of each of their calling threads.

This can lead to some unintuitive scenarios, like the following, for a queue `q`

```
Thread A: ---(q.enq(x))----(q.deq(y))
Thread B: -------------(q.enq(y))----
```
This is a sequentially consistent execution, because we can order the calls in such a way where the behavior is met, and they fall in the order of each thread:

```
B(q.enq(y)) -> A(q.enq(x)) -> A(q.deq())
```

And indeed the dequeud element will be y. That said, this is not a compositional property. Consider queues `p,q`.

```
Thread A: --(p.enq(x))-----(q.enq(x))----------(p.deq(y))
Thread B: --------(q.enq(y))--------(p.enq(y))---q.deq(x)
```
 
Claim 1: each object respects sequential consistency. This is clearly true. Consider the following orders:

```
# Object p
B(p.enq(y)) -> A(p.enq(x)) -> A(p.deq(y))

# Object q

Same as above
```

Claim 2: the entire system does not respect sequential consistency. Proof by contradiction. Certainly we need that

```
B(p.enq(y)) -> A(p.enq(x)) -> A(p.deq(y))
```

But we also need that

```
A(q.enq(x)) -> B(q.enq(y)) -> B(q.deq(y))
```

And so `B(p.enq(y)) -> A(p.enq(x))`. But we also have that 

```
B(q.enq(y)) -> B(p.enq(y))
A(p.enq(y)) -> A(q.enq(x))
```

And therefore `B(q.enq(y)) -> A(q.enq(x))` and this offers a contradiction. Logically, A needs to enqueue to `q` before B does, but B needs to enqueue to `p` after A does. 

So `A` needs to wait until after `B` enqueues to `q` to make its first move. But `B` needs to wait until after `A` enqueues to `p` to make its first move!

## Linearization

It is a problem that the above is not compositional! So we introduce another correctness propertly to resolve this...
The following correctness principle is called linearizability: Each method call should appear to take effect instantaneously at some moment between its invocation and response.

Every linearizable execution is sequentially consistent, but not the other way around. So now our initial example no longer holds.

To show a given object is always linearizable we will usually identify for each method a *linearization* point where the method takes effect.

Remark: sequential consistency is good for standalone systems (i.e hardware memories) where composition is not an issue. Linearization is nicer for components of large systems.

## Blob of notation 

Def: a *History* is a finite subsequence of method invocation and response calls. A subhistory is a subsequence of a history.

Each method invocation is written as `<x.m(a*)A>` where `x` is our object, `m` is the method, `a*` are the arguments, and `A` is the thread making the call

Each method response is written as `<x:t(r*)A>` where `x` is the object, `t` is either "Ok" or an exception, `r*` is the list of returned values, and `A` is the thread.

We say that a response matches an invocation if they have the same object and thread. Thus, a method call is a matching invocation and response. `complete(H)` is the subsequence of `H` consisting of all matching invocations and responses.

Def: we say that a history H is *sequential* if the first event is an invocation, and each invocation save the last is immediately followed by a matching response.

Def: `H|A` is the thread subhistory. Similarly, an object subhistory is denoted `H|x`. A history `H` is well formed if each thread subhistory is sequential.

Recall that a partial order is an ordering that is non-reflexive and transitive. A total order `<` is a partial order such that for all `x,y`, either `x < y` or `y < x`

Note that a partial order can always be extended to a total order (i.e. if there is a partial order `->` on the set `X`, then there exists a total order `<` such that `(x->y) => (x < y)`)

We say that a method call `m_0` precedes a method call in a history `H` `m_1` if `m_0`'s response event occurs before `m_1`'s invocation event. This is written as

```
m_0 ->_H m_1
```

Clearly, this is a partial ordering. Notice that if `H` is sequential, then `->` is a total ordering.

This gives us the following formal definition of linearizability. A history `H` is linearizable if it has an extension `H'` and there is a legal sequential history `S` such that...

1. `complete(H')` is equivalent to `S` (all the same invocations and responses occur, but not necessarily in the same order)
2. `m_0->_H m_1` => `m_0->_S m_1`

We refer to `S` as a linearization of `H`. Note that this extension captures the idea that we may have made some method invocations without having received a response.


**Theorem**: `H` is linearizable iff for each object `x`, `H|x` is linearizable.

Proof: pick a linearization for each `H|x`. Let `R_x` be the corresponding sets of responses that get appended to each one of these, and the history extension `H'` be `H` with all of these elements appended. Then, proof by induction on the number of method calls.

If there is 1 method call then certainly we have a linearization.

Otherwise pick a method all `m` that dominates all other method calls for the `->` comparator. If we remove it from `H'` then we have a linearization `S` (as we have one less method call) and we can just add this method call to the start of that linearization without posing any problems to our linearization definition.

###Nonblocking: 
a pending invocation of a total method (defined for all valid object states) is never required to wait for another pending invocation to complete.

Theorem: let `inv(m)` be an invocation of a total method. If `<x inv P>` is a pending invocation in a linearizable history `H`, then there exists a response `<x res P>` such that `H * <x res P>` is linearizable. The idea behind this is that, if we are in any state where we have invoked this total function, it can always terminate (and return) immediately, regardless of the other method calls that have been made.

The concern would be that this method would have to wait in order to run.

Proof:
The critical detail is that a linearization `S` will never contain only one of the invocation or the response. It's all or nothing. 

Case 1:
`S` contains `<x inv P>` and `<x res P>`. Claim: `S` is a linearization of `H * <x res P>`. Our concern here would be that, in adding a new method invicaton `m = <x inv P>, <x res P>` and then
A) some other event that dominates this in `H * <x res P>` does not dominate it in the linearization. This isn't a concern because domination depends only on the start time.
B) this dominates some other event (this never happens - as this method call ends last it dominates nothing)

Case 2:
`S` containts none of these. And so we can form `S'` = `S * <x inv P> * <x res P>`, and as, in `H * <x res P>`, the method call that we have just added dominates nothing, this will succeed. We are allowed to do this expressely because the method is total - there is no concern related to bringing this method invocation to a potentially new state.

Note that this doesn't proclude us from imposing our own blocking in the actual invocation of the method! For example, with a simple lock based queue.

What is thread preemption? Answer: when a thread gets its priority taken away.

A method is thus wait free if it guarantees that every call to it will finish its execution in a finite number of steps (for example, the doorway section bakery algorithm). We are bounded wait free if there is a bound on the number of steps we can take. This is an example of a nonblocking progress condition: one thread that is held up does not prevent the others from advancing. 

This is nice, but can be inefficient.

A method is lock free if if guarantees that infinitely often some method call finishes in a finite number of steps. Thus, admitting the possibility that some thread will starve.

## Dependant Progress Conditions

Recall the notion of deadlock-free and starvation-free locks from before. Note that these are dependant conditions - they depend on guarantees provided by the compiler. Therefore, they can be not so performant in, say, a case where a thread is often preempted.


Dependant nonblocking progress conditions:
Def: a method is obstruction free if from any point in which it executes in isolation, it finishes in a finite number of steps. This critically does not guarantee progress when multiple threads execute concurrently. In practice, this only stops threads that are in conflict, and can be resolved through a *back off* mechanism (a thread detecting a conflict pauses to give other threads time to process)

## The Java memory model

Not sequentially consistent when working with shared objects - this would eliminate a lot of the speedups that a compiler can provide. However, it satisfies the *Fundamental Property* of relaxed memory models: if a program's sequentially consistent executions follow a certain set of rules, then every execution of this program will be sequentially consistent.

We focus on a fundamental set of these rules that should be enough for most purposes:

First, we present an example of a broken idiom - double checked locking.

```Java
public static Singleton getInstance()
{
    if(instance == null)
    {
        synchronized(Singleton.class)
        {
            if(instance == null)
            {
                instance = new Singleton();
            }
        }
    }
    return instance;
}
```

Although it would seem like we first initialize the singleton object, and then assign it to instance, in practice, this might happen out of order. This would make a partially initialized instance class visible to other threads!

Therefore, we would suggest just using a `volatile` boolean to indicate when this operation is complete.

Furthermore, a thread is not required to publish its local changes to the public memory!

Redbone

We also have a number of synchronization events. These aren't just atomic / locking operations. They also pertain to reconciling a threads local memory with the shared memory, and likewise invalidation of a threads caches. We discuss some of these synchronization events...

### Locks and Synchronized Blocks

This can be done explicitly (locking an actual lock) or implicitly (a `synchronized` block). But in either case, the behavior and the guarantees are the same.

If all accesses to a field are protected by the same lock, then read-writes to that field are linearizable. When a thread releases a lock, it writes all modifications to local memory to working memory. Similarly, when a thread enters the lock, it invalidates its shared memory.

### Volatile Fields

Linearizable - single reads / writes are like acquiring a lock.


