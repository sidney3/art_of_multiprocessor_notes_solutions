# Chapter 3 Exercises

## 22: Explain why it is that quiescent memory is compositional

Recall: an object `x` satisfies quiescent consistency if the following property is true: any time an object becomes quiescent its execution is equivalent to some sequential execution of the completed calls. A nice example of a structure that works with this would be a shared counter.
v

We begin by proposing a more formal definition of this property. A history `H` is said to satisfy quiscent consistency if there exists an extension `H'` and a sequential history `S` such that
1. `complete(H')` is equivalent to `S`
2. If `m_0 ->_H m_1` are methods acting on the same object and there exists a period of quiescence between `m_0` returning and `m_1` being invoked, then `m_0->_S m_1`

Proof by induction on the number of methods called:

k = 1: and result is clear

k > 1: again, find dominating method, remove it, put it at the front of the new quiescentialization (?) and you're good.

## 23: Give an example of an execution that is quiescently consistent but not sequentially consistent, and then the reverse.

Quiescently consistent but not sequentially consistent:

```
ThreadA----q.enqueue(x)q.dequeue(-)
```

As there is no period of quiscence between these methods, they can occur in any order, and so we choose the order in which we dequeue first

Sequentially consistent but not quiescently consistent

```
ThreadA---q.enqueue(x)----------------q.dequeue(y)
ThreadB----------------q.enqueue(y)--------------
```

We still respect sequential consistency with the following ordering of events:

```
ThreadB(q.enqueue(y)) -> ThreadA(q.enqueue(x)) -> ThreadA(q.dequeue(y))
```

But we notice that there is a period of quiescence between each of these method calls, and so the linear execution is the only one possible.

## 24: For each history shown in Figs 3.13, 3.14, is each sequentially consistent, quesciently consistent, Linearizable?


Figure 3.13
```
ThreadA ------__r.read(1)__--------------
ThreadB -----___r.write(1)__--r.read(2)--
ThreadC --------_r.write(2)_-------------
```

The following ordering satisfies all forms of consistency.
```
B_W(r = 1) -> A_R(r == 1) -> C_W(r = 2) -> B_R(r == 2)
```

Figure 3.14
```
ThreadA ------__r.read(1)__--------------
ThreadB -----___r.write(1)__--r.read(1)--
ThreadC --------_r.write(2)_-------------
```

The following order satifies all forms of consistency
```
C_W(r = 2) -> B_W(r = 1) -> A_R(r == 1) -> B_R(r == 1)
```

## 25: Is the following condition equivalent to sequential consistency?

For a history `H`, there exists an extension `H'` and a sequential history `S` with `complete(H')` is equivalent to `S`.

Very unclear question: we never receive a formal definition of equivalent... Assuming that equivalent means "contains all the same elements," then not at all... We could just reorder the method calls to an order


## 26

The fundamental issue with this queue is that we set shift up the `tail` pointer before anything is actually enqueued...
And the way the code tries to resolve this is with a check to see if the value is null, but this feels intuitively like a bad solution.

The sticky situation we can have is a case where the state of the list looks like this:

```
[null, x]
```

(i.e. we have nothing in slot zero and we have an element at slot `x`). This can give us the following scenario:

```
ThreadA: _____enq(y)_______-----
ThreadB: --_enq(x)--------------
ThreadC: ----------deq(-)_------
```

When we run that `deq` from `threadC`, we have previously ran an equeue that *finished* (that from `threadB`). But the head is still `0`, and that is still null (because we say that `threadA` has successfully executed his CAS but not his assignment).

**check these two with computer**

## 29: Is the following equivalent to saying that object `x` is wait-free?

For every infinite history `H` of `x`, every thread that takes an infinite number of steps in `H` completes an infinite number of calls.

Yes - same logic as below


## 30: Is the following equivalent to saying an object `x` is lock free?

For every infinite history `H` of `x`, an infinite number of method calls are completed.

Yes - an infinite number of method calls must have been completed, each thus in a finite number of steps.
 
## 32

Not on line 16. If we set up our threads like this:

```
threadA: --___enq(x)________
threadB: ----__enq(y)-------
threadC: ------------deq(y)-
```

we have an issue (note that we say that `A` hits its linearization point before `B`, but `B` gets its write in first) 

Not on line 16. Say `threadA` gets its index write in first but `threadB` gets its item set first. Then the reader will get
the item with its index set first (assuming they both resolve).


