# Memory Order

Some notes on memory order:

`std::memory_order_acquire`: no reads or writes can be re-ordered to BEFORE this load

`std::memory_order_release`: no reads or writes can be re-ordered to AFTER this store
Any memory operations that occur before this store actually do get completed before this store.
AND any such changes will be visible to `acquire` operations (this is the really critical part that we were missing)

`std::memory_order_consistency`: establishes a modification order consistency (i.e. all threads will see the same order) and atomicity (linearity)


So an example like

```cpp
std::atomic<int> x{ 42 };

// ensures that no prior reads get re-ordered to after this
x.store(5, std::memory_order_release);

if (x.load(std::memory_order_acquire) == 42)
{
    end_the_world();
}

```
```cpp
std::atomic<int> x{ 42 };

// ensures that no prior reads get re-ordered to after this
x.store(5, std::memory_order_relaxed);

if (x.load(std::memory_order_relaxed) == 42)
{
    end_the_world();
}

```
