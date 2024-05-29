# Chapter 4 Exercises

## 34: will the following implementation of a MRSWRegister be safe?

```cpp
template<typename T>
struct SafeSRSWRegister;

template<typename T, long N>
struct SafeMRSWRegister
{
    std::array<SafeMRSWRegister<T>, N> values;

    SafeMRSWRegister(T init)
        : SafeMRSWRegister(init, std::make_index_sequence<N>())
    {}

    void set(T new_val)
    {
        for(size_t i = 0; i < N; i++)
        {
            values[i].set(new_val);
        }
    }

    T get(size_t thread_id)
    {
        return values[thread_id].get();
    }

private:
    template<size_t ... Is>
    SafeMRSWRegister(T init, std::index_sequence<Is...>)
        : values{ {static_cast<void>(Is), init}... }
    {}

}
```

This is safe: if we make subsequent writes and then reads, then we will clearly get the desired result. Similarly, as each register in the table is safe, if we make a read while writing, then certainly a valid value will be output (because the register is safe)

## 35: consider the same impl as above but for a boolean register and with regular SRSW registers. Will it be a regular MRSW register?

1. Definitely we won't be reading from the future
2. Once a write to all registers finishes, we have the regular guarantees for each register and so we have them for the whole system.

## 36: Consider the following atomic MRSW implementation. Will it still be atomic if we replace the atomic SRSW registers with regular SRSW registers?

Recall that this is the table implementation. True.

We want to show that `(R_i -> R_j) => (i <= j)`. Proof by contradiction: assume `i > j`. `R_i -> R_j` and so the separate write that occurs in the read of value `i`, that is, setting the value in the other thread's table, successfully happens. We're regular so this change will definitely get seen (instead of any prior writes), and we assume that `R_i` is the latest execution by its respective thread before `R_j`...

## 37: Give an example of a quiescently consistent register execution that is not regular

Very easy:

```
ThreadA---____W_i____-___W_j___-----R_i---
ThreadB----------___R_i___----___W_x_____
```

And then we have 

```
W_i -> W_j -> R_i
```

## 38:
```cpp
struct atomicMRSWBoolRegister;

/*
Single write, MRSW atomic register
*/
template<long N>
struct acmeRegister
{
    std::array<atomicMRSWBoolRegister, 3 * N> arr;
    acmeRegister() : acmeRegister(std::make_index_sequence<3*N>())
    {}

    void write(int x)
    {
        std::vector<bool> x_as_bits = int_to_bits(x);

        for(size_t i = 0; i < N; i++)
        {
            arr[i] = x_as_bits[i];
        }
        for(size_t i = 0; i < N; i++)
        {
            arr[i + N] = x_as_bits[i];
        }
        for(size_t i = 0; i < N; i++)
        {
            arr[i + 2*N] = x_as_bits[i];
        }
    }
    void read()
    {
        vector<bool> x1(N), x2(N), x3(N);

        while(x1 != x2 && x2 != x3)
        {
            for(size_t i = 0; i < N; i++)
            {
                x1[i] = arr[i];
            }
            for(size_t i = 0; i < N; i++)
            {
                x2[i] = arr[N + i];
            }
            for(size_t i = 0; i < N; i++)
            {
                x3[i] = arr[2*N + i];
            }
        }

        if(x1 == x2)
        {
            return to_int(x1);
        }
        else
        {
            return to_int(x2);
        }
    }

private:
    template<size_t ... Is>
    acmeRegister(std::index_sequence<Is...>) :
        arr{ {std::static_cast<void>(Is), 0}... }
    {}

};
```

This will spin at most once (if we are not currently writing then of course the values will get agreed upon). We also could get away with only using an array of size `2*N`...

## 39 Nope:

We can totally do reads "from the future". Just make two distinct writes and during the second make a read and we can get any value.

## 40 Does the peterson algorithm still work with regular registers?

No:
```
ThreadA:--------write_A(victim = A)____________________________________________--read_A(victim == B)
ThreadB:------------------------------write_B(victim = B)--read_B(victim == A)---------------------
```
Critically, note the out of order read!

The way this could practically arise is as follows -> `A` writes the register for `A`, `B` overwrites *both* registers (the one for `B` and the one for `A`). Then, `A` tramples the write that `B` made for the `B` register.
