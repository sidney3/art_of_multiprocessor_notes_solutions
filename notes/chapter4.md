# Foundations of Shared Memory

Let us start with the building blocks of shared memory -> the register. We can categorize register memory states as follows:

Note: safe / regular only have a single writer

1. Safe (bad name)

Read after a write has completed is always correct. Read while a write is happening can be any defined value for register.

2. Regular

Safe, with the additional trait that if `v_0` is the initial state before the overlapping read/write, and `v_1,...,v_k` are the values that the register gets set to during the overlapping read/write, then each read is one of `v_0,...,v_k`

Alternatively, a rigorous definition:

1. It is never the case that `R_i -> W_i`
    note that this also implies that each corresponds to a valid write that has already happened.
2. It is never the case that for some `j`, `W_i -> W_j -> R_i`

We can interpret this as quiscent consistency. We CAN have something like this:

```
ThreadA: ---------R_j-----R_i----
ThreadB: -W_i----_____W_j______--
```

Clearly not linearly consistent -> we need to place the write of value j before the read of value j, but this means that the read of value i is impossible. But it IS possible with the following quiescently consistent reordering:

`B(W_i) -> A(R_i) -> B(W_j) -> A(R_j)`

3. atomic

All reads / writes are linearizable, with a total order defined on the writes

Rigorously, we satisfy all the regular conditions, along with the following

3. If `R_i -> R_j` then `i <= j`

## Some notation:

`W_i` will refer to the unique value `v_i` that is the ith write. `R_i` will refer to a read of this value `v_i` (and thus is not necessarily unique)

Additionally, a field `mumble` will be called `s_mumble` if it is safe, `r_mumble` if it is regular, and `a_mumble` if it is atomic

And we now provide some implementations:

```cpp
template<typename T>
struct IRegister
{
public:
    virtual T read() = delete;
    virtual void write(T) = delete;
};
```

### MRSW Safe register
```cpp
/*
initialized to zero
*/
struct SafeBooleanMRSWRegister : IRegister<bool>
{
    SafeBooleanMRSWRegister(size_t capacity) : s_table(capacity, false), _capacity(capacity)
    {
    }
    bool read()
    {
        return s_table[std::this_thread::get_id()];
}
    void write(bool b)
    {
        for(size_t i = 0; i < capacity; i++)
        {
            s_table[i] = b;
        }
    }

private:
    std::vector<bool> s_table;
    const size_t _capacity;
    
};
```

Certainly if we follow sequential execution, this will be respect. Otherwise, the register can return any value.

ASIDE ON THREAD_LOCAL: `thread_local` can only be used in C++ on static data types. This makes sense - it is statically allocated at the start of thread lifetime so it can't really be dynamically allocated (as it would be in a class that we could make multiple instances of). The solution is sick - for a case where we want thread local instances of a type `T` in a class `A`, we can add a member like this:

```cpp
struct A
{
    static thread_local std::unordered_map<A*, T> tl_t;
};
thread_local std::unordered_map<A*, T> A::tl_t{};
```
The only issue with this is removing `A*` values (if instances of `A` are shared across threads) because then we can't do it in the destructor. But this is fine...

```cpp
class RegBooleanMRSWRegister : IRegister<bool>
{
public:
    RegBooleanMRSWRegister(size_t capacity) : s_value(capacity)
    {}

    void write(bool x)
    {
        if(x != last[this])
        {
            last[this] = x;
            s_value.write(x);
        }
    }
    bool read()
    {
        return s_value.read();
    }
    
private:
    static thread_local std::unordered_map<RegBooleanMRSWRegister,bool> last;
    SafeBooleanMRSWRegister s_value;
};
thread_local std::unordered_map<RegBooleanMRSWRegister*, bool> RegBooleanMRSWRegister::last{};
```

Lemma: this is a regular register.

Certainly if we have a write and then a read with a period of quiescence between, we will be fine. The interesting question of course, comes up when we have concurrent reads and writes. We have two cases:

Case 1: the prior state is `0` and the write is to `1` - then any return is legal and certainly our register will never set the interal value to something other than `0` or `1`.

Case 2: the prior state is `0` and the write is to `0`. This is a case where our safe register could screw us - if we have a concurrent write and read we could of course have any value. However, our one writing thread will not perform a write! This is the same logic as if the prior state is `1`.

**Question**: how do we get the preallocated size without the initialization?
```cpp
template<unsigned M>
struct RegMValuedMRSWRegister 
{
public:
    RegMValuedMRSWRegister(size_t capacity) 
    : value(M, RegBooleanMRSWRegister{capacity})
    {
        value[0] = true;
    }
    void write(unsigned x)
    {
        assert(x < M);
        value[x].write(true);

        for(int i = x - 1; i >= 0; i--)
        {
            value[i].write(false);
        }
    }
    unsigned read()
    {
        for(int i = M - 1; i < M; i++)
        {
            if(value[i])
            {
                return i;
            }
        }
        __builtin_unreachable();
    }
private:
    std::vector<RegBooleanMRSWRegister> value;
};
```

And clearly when we make a read we will either get the last value or the new value that just got written.

Can we create a scenario like follows?:

```
ThreadA: ---------R_j------------
ThreadB: -----------------R_i----
ThreadC: -W_i----_____W_j______--
```

Yes -> think about how the underlying safe register actually works.

Say that `W_j < W_i`. Then we can be in a scenario where we have just called `value[W_i].write(true)`. The `true` flag IS set to true for `threadA` when he makes his read, but it is NOT yet set for `threadB` when he makes his read.

So we make a note that if there is only a single writer / reader, this is technically atomic. However, this is hard to reason about.

## Implementing Atomics

### SRSW
Remember that we are going for that third correctness policy. So our strategy will be to use a regular register, but each time we make a read, we will verify that this read is indeed the last one.

```cpp
template<typename T>
struct StampedValue 
{
public:
    StampedValue(T&& init, long stamp = 0)
        : _stamp(stamp), value(std::make_unique<T>(std::move(init)))
    {}
    static StampedValue<T> max(const StaticValue<T>& lhs, const StaticValue<T>& rhs)
    {
        return (lhs.stamp > rhs.stamp) ? lhs : rhs;
    }
    long stamp;
    T value;
};

/*
Note that for notational convenience we will refer to a thread local variable with the prefix thread_local. Although this is not valid C++ it is possible to recreate this behavior with a std::unordered_map as mentioned above

And we assume sane default values. See the experiments folder for a nice implementation of this
*/
template<typename T>
class AtomicSRSWRegister : IRegister<T>
{
    thread_local StampedValue<T> last_read;
    thread_local long last_stamp;
    StampedValue<T> r_value;

    T read()
    {
        StampedValue<T> local_last_read = last_read.read():
        StampedValue<T> local_value = r_value.read();
        StampedValue<T> furthest_value = StampedValue<T>::max(local_last_read, local_value);
        last_read.write(furthest_value);
        return furthest_value.value;
    }
    void write(T new_value)
    {
        long stamp = last_stamp.read() + 1;
        r_value = StampedValue<T>(stamp, new_value);
        last_stamp.write(stamp);
    }
};
```

The register is regular, so at a base we are regular. And it is impossible to have out of order reads to the above is atomic.

As for how to make this support multiple readers, a first thought might be to use the same techique from the first approach -> have an array of values for each thread. But this does not give us our atomic result that we would like...

```
ThreadA-----------------R_i---
ThreadB-----------R_j---------
ThreadC--W_i-----_____W_j_____
```

Is still possible if the array value for for `threadB` gets updated, but the one for `threadA` does not before the termination of `W_j`.

That said, we can address this problem by having an earlier reader "help out" later threads by telling them what it read! So keep a timestamped local value of what we just read? Not at all! If we have `n` writers, we make an `n * n` table. When the writer makes a write, it does so to each cell `(i,i)` for each `0 <= i <= num_threads`. When a reader with id `i` makes a read, it first extracts the latest timestamped value from all of its cells (all cells in row `i`). And then it goes through each other thread `j` and writes the updated read value in cell `(j,i)`. Certainly each cell will be SRSW. So we verify all our conditions:

1. Each register we read from is atomic
2. We always get the latest read because we always check what the writer has written (if a writer has written something before the time we start a function, we are going to get that)
3. `(R_i -> R_j) => (i <= j)`: each time we make a read, we know that it is the latest timestamped read since we began function execution.

```cpp
template<typename T>
struct AtomicMRSWRegister : IRegister<T>
{
    thread_local long last_stamp;
    std::vector<std::vector<TimestampedValue<T>>> a_table;
    const size_t capacity;

    AtomicMRSWRegister(size_t capacity)
    {
    // ... initialize to sane values
    }

    T read()
    {
        size_t me = std::this_thread::get_id();
        TimestampedValue<T> result = 
            table[me][me].read();

        for(size_t i = 0; i < capacity; i++)
        {
            result = TimestampedValue<T>::max(
                result,
                table[me][i].read()
            );
        }

        for(size_t i = 0; i < capacity; i++)
        {
            table[i][me].write(result);
        }
        
        return result.value;
    }
    void write(T new_val)
    {
        long new_stamp = last_stamp.read() + 1;
        TimestampedValue<T> new_stampval{new_stamp, new_val};
        for(size_t i = 0; i < capacity; i++)
        {
            table[i][i].write(new_stampval); 
        }
        last_stamp.write(new_stamp);
    }
};
```

And this flows really naturally into the MRMW implementation, where we will use the following strategy (assuming we have `N` writers)

Make a table of `N` `MRSW` atomic registers. Each writer will have a register that belongs to it, and each slot in the table will hold a timestamp object. 

The read operation is as follows: get the latest timestamp from the entire table and add 1 to it. Then, in our own slot, write the requerted new value with this new maximum timestamp.

The write operation is as follows: get the latest timestamped from the writers cells and return it! 

Devastatingly simple...

```cpp
template<typename T>
struct AtomicMRMWRegister : IRegister<T>
{
    std::vector<TimestampedValue<T>> a_cell;
    AtomicMRMWRegister(size_t capacity)
    {
    //...
    }

    void write(T new_value)
    {
        size_t me = std::this_thread::get_id();
        long latest_stamp = 0;

        for(size_t i = 0; i < capacity; i++)
        {
            latest_stamp = max(
                latest_stamp, 
                a_cell[i].read().stamp;
            )
        }
        a_cell[me].write(TimestampedValue<T>(new_value, latest_stamp));
    }

    T read()
    {
        size_t me = std::this_thread::get_id();
        TimestampedValue<T> res = a_cell[0].read();
        for(size_t i = 1; i < capacity; i++)
        {
            res = TimestampedValue<T>::max(
                res,
                a_cell[i].read()
            );
        }
        return res;
    }
};
```

We verify our three conditions:

1. Clear - we can never read values from the future
2. If `W_i -> W_j` then we must have that the stamp index for `W_j` is greater than that for `W_i` and thus it will get chosen over `W_i` by the reader.
3. If one read gets a different value from another AND starts after it, it must have been that this new value that was read had a higher value than the original and thus was written later (as we always have that a write that happens after another will have a higher value).

## Snapshots

We are now interested in the notion of taking a snapshot of registers:

that is, given a number of `MRSW` registers, we want to create a structure that can support the following methods:

*is this how you  make an interface in C++?*
```cpp
template<typename T, long N>
struct ISnapshot
{
    /*
    Set the register that corresponds to thread_id to val
    */
    virtual void update(T val, size_t thread_id) = delete;

    /* 
    get the values for all registers. Note: ALL STATES must have existed in the same state at some point.
    */
    virtual std::array<T, N> scan() = delete;
};
```

So we can say that whatever wait free implementation we build will be equivalent to the following implementation...
```cpp
template<typename T, long N>
struct SequentialSnapshot : ISnapshot<T, N>
{
    SequentialSnapshot(T init_value) : m()
    {
        std::fill(values.begin(), values.end(), init_value);
    }

    void update(T val, size_t thread_id)
    {
        std::unique_lock<std::mutex> lk(m);
        values[thread_id] = val;
    }
    std::array<T,N> scan()
    {
        std::unique_lock<std::mutex> lk(m);
        return values;
    }
private:
    std::mutex m;
    std::array<std::atomic<T>, N> values;
};
```

One way for us to tell if we are in our desired state is if we have two subsequent collects that are the same - clearly we had no change between our two collections, and so we are in the same state as in the moment between these two collections. This is a good enough idea that we name it something - a "clean collect."

This suggests a simple obstruction free implementation where we just use atomic MRSW registers. We note that we store, in these values, stamped items. We define a method collect that iterates through the registers, making a copy of each item. Then The update method is just a setter, and the snapshot method repeatedly calls `collect()` twice, and compares the two copies.

This is evidently not wait free, but the idea we are going to use is as follows. If we have a scanning thread `A` (that is between scans) sees a thread `A` move _twice_, then we know that `B` executed a complete `update()` call within the interval of our scan, and so we should use `B`'s snapshot.

Here is the code for this, because I think it would help me to write it out (because this is highly nontrivial), and then an explanation.

```cpp
template<typename T, long N>
struct StampedValue 
{
    StampedValue(std::array<T,N> snap, int stamp, T value)
    : snap(snap), stamp(stamp), value(value)
    {}
    std::array<T, N> snap;
    int stamp;
    T value;
};

template<typename T, long N>
struct WaitFreeSnapshot : ISnapshot<T,N>
{
    //note : MRSW atomic
    std::array<std::atomic<StampedValue<T,N>>, N> values;

    WaitFreeSnapshot(T initial)
    {
        // initialize all timestamped values using initial
    }
    std::array<StampedValue<T,N>,N> collect()
    {
        std::array<StampedValue<T,N>,N> res;
        for(size_t i = 0; i < N; i++)
        {
            res[i] = values[i].get();
        }
        return res;
    }
    void update(T val, size_t thread_id)
    {
        std::array<T,N> local_scan = scan();
        long new_stamp = values[me].get().stamp + 1;
        values[me] = StampedValue<T,N>(local_scan, new_stamp,
        val);
    }
    std::array<T,N> scan()
    {
        std::array<bool, N> moved;
        std::array<StampedValue<T>, N> old_collection = scan();
        std::array<StampedValue<T>, N> new_collection;
        bool collection_mismatch = false;
        do
        {
            new_collection = scan();

            for(size_t i = 0; i < N; i++)
            {
                if(old_collection[i] != new_collection[i])
                {
                    if(moved[i])
                    {
                        return values[i].get().snap;
                    }
                    else
                    {
                        moved[i] = true;
                        collection_mismatch = true;
                        old_collection = new_collection;
                        break;
                    }
                }
            }
        } while(collection_mismatch)

        std::array<T,N> res;
        std::transform(
            new_collection.begin(),
            new_collection.end(),
            res.begin(),
            [](const auto& x)
            {
                return x.value;
            }
        )
        return res;
    }
};
```
Of course, if we move twice, then this means that the snapshot that this other thread will have of the world will be correct (assuming correctness of `scan()`).

What I'm more interested in is proving that this is wait free.

Claim: of many pending `scan()` calls, at least one will terminate. 

Proof:  Let `x` be an instance of `WaitFreeSnapshot`

Case 1: all threads that are inside `x` are in the scanning method. Assuming this state stays this way, then clearly one will finish in a single step (as we are no longer updating). If during the execution of this case, another thread comes in and updates, it must FIRST go to the scanning, and so we will still get the desired result.
Case 2: not all active threads are scanning. As a the non-scanning steps are finite, this will lead to case 1 in a finite number of steps.

As long as there is at least one completion of `scan()` in a finite number of steps from any state, we will always finish - because this means that, as we have a finite number of threads, *all* threads will finish scanning in a finite number of steps, or there are repeats (and once there are repeats we exit the system).

Pretty damn sick.

