# Written Exercises
(not coding)

## Exercise 3

Both Bob and Alice will both have a can that rests on the others porch. Again, for each can, if you have the string end of it, you can pull it down. And it you have the can end of it, you can put it back up.

Then, we follow the following protocol. When Bob is ready to put food out, he checks if his can is knocked down. If it is up, he waits until it gets knocked back down. Then, he puts the food out, picks his own can up, and knocks Alice's can down.

Alice's protocol is similar. When she wants to feed the dogs, she waits until her can is knocked down. Then, she releases her dogs. Once her dogs are back inside, she resets her own can, and then knocks Bob's can down.

**No bad case**: for Bob to enter the yard, his can must be knocked down. For Alice to release her dogs into the yard, HER can must be knocked down. So we claim that the two cans will never both be in a knocked down state. And before each can gets knocked down, the other always gets picked back up, so this is impossible. And while an individual's can is up, they aren't going to do anything.

**Good state**: infinitely hungry dogs will be fed forever. If Bob is waiting to feed the dogs, they will always return inside, and then Alice won't need to wait for anything - she will immediately release Bob. Same for the other way around - once we go outside we don't wait for anything.

## Exercise 4

### A:
we assign a single wise man. Each time they see the switch flipped to ON, they increase their own count and flip the other cup down. On their n-1th flip, they announce that they are done. For all others, the FIRST time they come to the switch and see that it is flipped off, they turn it on. Otherwise, they do nothing (if they see the switch flipped on, this does not count against their one flip).

### B:
Same as above, but we now will flip for the first two cases where we find the switch like this, and the wise man will announce that everybody has touched once he counts 2*(N-1) - 1 flips

## Exercise 5

The first says "red" if he sees an odd number of red hats, otherwise he says blue. This is sufficient information for the rest to figure out all that they need to.

## Exercise 6-8

Do these later
