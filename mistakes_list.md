Chapter 7: the rigorous definition given for an atomic variable, which does not agree with the "colloquial" definition offered.


Chapter 8: the initial Reader-Writer lock proposed allows multiple writer threads to enter at the same time. Below is the proposed writer lock:

```Java
protected class WriteLock implements Lock {
public void lock() {
    lock.lock(); 
    try {
        while (readers > 0) { 
            condition.await();
        }
        writer = true; 
        } 
    finally {
        lock.unlock();
    }
}
```

While this will prevent us from entering while there are readers who have the lock, any two writers could enter the lock at the same time (there is no check of the `writer`) variable by a writer.


