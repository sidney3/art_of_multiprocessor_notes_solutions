# Chapter 5 Exercises

## 47: Every n thread consensus protocol has a bivalent initial state

Proof by reduction: say only `2` of our threads choose to act. Then our scenario is identical to the 2 thread consensus protocol, which we know has a bivalent initial state

## 48: Same question as 47

## 49: Prove that in a critical state, one successor must be 0-valent, the other 1-valent.

A critical state is by definition bivalent...

## 50: Hint gives it away

## 51: Again, proof by reduction

## 52:

So we have a list n of binary consensuses -> thread i will propose 1 to consensus i, all other threads will propose 0.

It would be impossible to have two threads get 1: once we get 1 in response to a proposal we stop. But it is not guaranteed that this thread will work!
We could have that, for example, thread `n` goes ahead and proposes 0 for all the consensuses except the last, and then any other thread goes and proposes 0 for the last.

Instead, each thread proposes for only UP TO his value. And so by default the maximum thread will always win.
But then, the younger threads can't figure out what value to go by!

Maybe some kind of default tiebreaker? Furthest back non-null? Not possible.






## 53: prove a stack with methods pop and push (LIFO) has consensus number exactly 2

We have two thread consensus if we make a stack with bottom element = LOSE, top element = WIN. 

We don't have > 2 thread consensus: proof by contradiction:

Drive to a critical state. Switch on the two actions about to be taken by threads `A` and `B`

Case 1: pop, pop -> and these commute so we states where `A` goes first is indistinguishable to one where `B` goes first to `C`
Case 2: pop, push -> we choose to order as push -> pop so state is indistinguishable
Case 3: push, push -> and both `A` and `B` will need to pop to evaluate state so we get to two different univalent states with the same features to `C`

## 54: assume our queue has a peek() method that returns the first number in the queue. Prove that it now has consensus number infinity.

Use the following protocol: each thread pushes its own ID onto the queue, and then peeks at the first element to decide who to cede to.

## 





