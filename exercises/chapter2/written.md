# Chapter 2 Exercises

## Exercise 9

Def: a mutual exclusion algorithm satisfies **r-bounded-waiting** if `D_A^j -> D_B^k` implies `CS_A^j -> CS_B^(k+r)`. That is, if one thread completes the doorway step of the mutex algorithm `x` "steps" (there are `x` threads that complete the doorway step between) before another, then it will enter the critical section at most `x + r` steps after the other - i.e. it will get cut by at most `r` other threads. Is there a way to provide `r`-bounded waiting for the Peterson algorithm for some value of `r`?

Hard to interpret the question: if it is asking us to suggest a modification such that the r bound gets an UPPER bound, then:

No modification is necessary -> the peterson lock is already r-bounded. We have the following guarantee: if `threadA` completes the doorway before `threadB`, then `threadA` will enter the critical section before `threadB`. Assuming our thread has not yet entered the CS, `threadB` will only enter the critical section once some other thread sets the victim to itself or removes its flag. And the only thread that could do that is us, `threadA`. But we will only do that once we exit the CS.

So as the thread is deadlock free we must have that we will enter the CS before the other thread.

if this is askign us to suggest a modification that sets the lower bound for r (i.e. allows us to have jumps in the queue, then):

This is impossible -> the state of a thread that is waiting as compared to a thread that has finished waiting and has entered the CS is indistinguishable. In the logic of a program that raises `r` from 1 to some arbitrary value, we would need some sort of condition during the doorway that allows us to "jump" the other thread (not wait, while that thread continues to wait).

In addition to whatever counter we would need to implement for this, this would require us to know if that thread is waiting (in which case we might skip it), or if it is in the CS (in which case we should never skip it).


## Exercise 10


Recall that the definition of FCFS is as follows: a lock if FCFS if the following holds

```
(D_A^j -> D_B^k) => (CS_A^j -> D_B^k)
```
The question asks why we can't define this by
```
((first instruction executed)_A^j -> (first instruction executed)_B^k) => (Same as above)
```

The idea behind the proof to this answer shall be that we need to change the state to reflect that we are waiting. Otherwise, how would we be able to know who got served first?

Imagine the following scenario -> `thread A` executes its first instruction, priority gets transferred to `thread B`, and as the lock is deadlock free, `thread B` must be released after some arbitrary amount of time -> and therefore the lock needs to know that `threadA` executed its first instruction.

If this first instruction is a read, there is obviously no way that the lock can know this. If the first instruction is a write to a shared location, then we could just have that `threadA` makes its write, `threadB` overwrites this, and then there is no way for the lock to know to tell `threadB` to wait. The trickiest case is when they are both writing to seperate locations. This write would need to inform the lock to wait on `threadA` -> so we can't have the threads write the same message (this won't allow the lock to conclude anything). But to write a different message of meaningful value we would need to make a read first!

## Exercise 11

Mutual Exclusion: Yes. Proof:

Consider one thread that has just entered the critical section. We claim that no other thread will be able to enter the critical section. There are three states that another thread can be in:

1. Inside the innermost while loop - and thus this thread will remain there until the thread in the CS leaves, as busy never gets set to false
2. Inside the second to innermost while loop - and thus `turn != me`, because this thread must have been in this loop (not the innermost one), when the thread entered the CS, as in entering the CS this thread set `busy = true`
3. 

Deadlock free: No. Firstly, this will follow directly from the first result and the fact that we don't access `n` locations in memory (only 2 locations in memory are shared between all threads). But we can also just show the deadlock directly. Suppose threads `A` and `B`, and that `busy` is initially false.

```
write_A(turn = A)
read_A(busy == false) # true
write_A(busy = true)  # thread B is now trapped
write_B(turn = B)
read_A(turn == A) # false

# and now thread A and B are both trapped in the inner while loop
# with nobody to set busy to false
```

And this is a stronger result than starvation free.

## Exercise 12

Show that the filter program allows a thread `A` to overtake a thread `B` an arbitrary number of times, regardless of the position of `B` (assuming it is not in a critical section).

And for this we just need to use two different threads -> one that will be the "sacrifice" victim and the other that will advance. Certainly we can advance UP UNTIl the level that a given thread is on (as we are deadlock free), so the question that becomes, can we advance past this level? And we just need to advance two threads to the current level that `threadB` is on, one sets the victim to itself, and so the other is then able to advance.

## Exercise 13
