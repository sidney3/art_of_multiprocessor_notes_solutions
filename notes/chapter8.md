# Monitors and Blocking Synchronization

A monitor is an object with both a lock and an object stored inside of it.

Note: the following will be our interface for locks for the rest of the book:

```cpp
public interface Lock {
void lock();
void lockInterruptibly() throws InterruptedException; // excepts if interrupted
boolean tryLock(); // fails if lock if taken
boolean tryLock(long time, TimeUnit unit); // fails after some time
Condition newCondition(); // returns a condition associated with the lock
void unlock();
}
```

## Conditions

Motivation: we are working with a queue and now want to wait until there are more elements in the queue, and release the lock until that is no longer the case.

```cpp
Condition cond = mutex.newCondition();

mutex.lock();
while(!property)
{
    cond.await(); // wait for property
}
```
Calling `signal` on a conditional will thus of course wake up one thread. `signalAll` will wake up everybody. Note that we can also await until a specific time or for some time length.

Then the text presents a queue with two conditionals `notFull` and `notEmpty` that threads can wait on. This is what you expect...

Some practices to minimize threads getting forgotten about:

1. Always signal all processes waiting on a condition, not just one
2. Specify a timeout when waiting

## Reader Writer lock

Finally interesting. Motivation: it is fine for readers to play together, but we cannot have concurrent reads / writes, or writes / writes. So when a writer joins the lock, it locks others out.

Here is one suggestion!
```cpp
struct Qnode
{
    Qnode(bool is_reader) : is_reader(is_reader), locked(false) {}
    const bool is_reader;
    std::atomic<bool> locked;
};
struct ReaderWriterLock
{
    thread_local Qnode myNode;

    std::atomic<Qnode> tail;

    void lock(bool is_reader)
    {
        myNode.set(Qnode{is_reader});
        Qnode pred = tail.get();

        while(!tail.compare_exchange_strong(pred, myNode));

        myNode.locked = true;

        if(is_reader && pred.is_reader)
        {
            return;
        }

        while(pred.locked());
    }
    void unlock(bool is_reader)
    {
        Qnode node = myNode.get();
        node.locked = false;
        myNode.set(pred);
    }
};
```

Preferred would be just making two local nodes, each one that gets chosen based off if we are a reader or not.

The text suggests a wrapper for two locks that synchronizes with a third lock.

We first lock that third lock to synchronize our state. The reader will `await()` while there is a writer, and the writer will `await()` while there is `>1` reader. Note that the author makes an error in the text: it is more than possible for >1 writer to grab ownership of this lock...

But in either case, when the writer unlocks he signals to the readers, and when the last reader (we keep a reader counter) unlocks, we signal to all the writers.

And our technique for making it more fair is just to have the `writer` not release the main lock, and so readers continue to trickle out, while none come in. This is NOT what the code says but whatever...

## Designing a re-entrant lock

We want to design a lock that can be acquired multiple times by the same thread.

This can be done with an internal lock that we lock every time we want to manage state. We keep  a field for the current owner and for the number of times they have entered. 

Another mistake: in the `lock()` method we never unlock our lock

## Semaphores

Generalization of mutexes: we now allow a capacity of threads to enter the CS, rather than just one. And obviously this has a trivial implementation with an internal lock to synchronize state of the struct...

Not going to do the exercises because they are stupid
