# Chapter 9 Exercises

## 100
Describe how to modify each linked list algorithm if the hash codes are not unique.

The big thing that changes is the searching step of the algorithm. Now, we search while `curr.code <= code` and do an equality check before each loop to see if our `curr` element is the target element. 

```
pred = head;
curr = pred.next;

while(curr.code <= target_code)
{
    if(curr.item == target_item)
    {
        break;
    }

    // whatever other logic we would do
}
```

## 101:

Explain why the fine grain locking algorithm is not subject to deadlock:

Because we always lock the locks in the same order. Deadlock can only arise when we have a two way dependency: A is waiting for B AND B is waiting for A. But this will never be possible if we always lock the locks in the same order. Assume A is waiting for B. Then B has locked a lock that has higher precedence than any lock A could have locked and so B cannot be waiting for A.

## 102:

Explain why the fine grain locking's `add()` method is linearizable

Linearization point of the last lock of `curr`. If another thread observes the state after this point, they will first have to wait for this lock to get unlocked, and then will certainly observe that the node has been added. Likewise, any change before this point will be observed by the adding node. If another node has inserted the value that we want to insert before we lock this node, then they must have locked the node as well.

## 103:

Explain why optimistic / lazy locking algorithms are deadlock free: 

Same reasoning as 101

## 104:

Show a scenario in the optimistic deadlocking algorithm where a thread is forever trying to delete a node.

The locking is starvation free and so the only area where this could happen would be during the `validate`. Basically, `validate()` repeadetly fails, but then we return back to it.

So the scenario would look like this: some other thread is constantly deleting and re-adding the node that we want to delete. 

1. We get `pred, curr` such that `curr.code >= code`. 
2. Another node deletes `curr`
3. We lock `pred, curr`. We validate them, our validation fails
4. Jump back to step 1


## 105:

Implement `contains()` for the fine grained synchronization set. Note that we do have to use locks, because our variables are not stated to be atomic, and it is UB to perform a write and a read from a non-atomic variable at the same time, which would be possible if we didn't lock.

```cpp
bool contains(T obj)
{
    using ul = std::unique_lock<std::mutex>;

    int code = hash(obj);
    auto pred = head;
    auto curr = pred->next;

    ul pred_lock{pred->lock};
    ul curr_lock{curr->lock};

    while(curr->code < code)
    {
        pred_lock.unlock();
        pred = curr;
        curr = curr->next;
        ul next_lock{curr->lock};

        pred_lock.swap(curr_lock);
        curr_lock.swap(next_lock);
    }

    return curr->code == code;
}
```

Linearization point will be the final lock of the `curr` node's lock.

## 106:

No: now we can have deadlock as we are locking the locks in an inconsistent order.

## 107:

Show that `tail` is always reachable from `predA` from the optimistic locking list.

This is a relatively cool propertly of the list:

1. We will never remove `tail` (sentinel node)
2. Whenever we remove a node, we just shorten the path from its `pred` to `tail`

Think about how the removal / addition algorithms actually work. When we add a node, that node gets INSERTED with the `next` field already well defined. When we remove a node, we just set the `pred.next` field one up. So we will never do anything to obstruct the path of a node to the `tail`, as the only way to make one node `B` not reachable from another node that is before it is just to remove `B`.

## 108:

Show that in the optimistic algorithm, `add()` only needs to lock pred 

Also super cool. Let's think about the ways that `add()` could go wrong. 

If it coincides with another `add()` call, then that call could `add()` a node after `pred` (mess with `pred.next` field) or could add a node after `curr`. In the latter case, we don't actually care: the pointer to `curr` is unchanged regardless of if somebody adds a node to it.

If it coincides with a `remove()` call, then the concern would be if we removed either `pred` OR `curr`. However, in order for `remove` to succeed, it needs to have both the `curr` and `pred` nodes to be removed! And so it won't remove `pred` or `curr`: it would need to have the lock for `pred` in order to pull off both of these calls!

## 109:

This fucks with our notion of linearizability because we have that the linearization points for `add()`, `remove()` is when we LOCK thefinal lock, where this messes with that. And there is no linearization point that we'd be able to assign for this latter functions: because `contains()` would always be able to conclude either true or false.

## 110:

If we mark a node as removed by setting its `next` field to `null`, would this work in the lazy and lock free algorithms?

Lazy:

No, because we have that other nodes can potentially traverse through the about to be removed node while it is marked if they are interested in a node past it. This would make that impossible.

Lock free:

Totally: no node will ever traverse past a marked node, and so it doesn't matter how we mark it. Though this would require modifying `contains()` to use our `find` function.

## 111:

Can `predA` ever be unreachable in the lazy algorithm?

Totally: if we acquire `pred,curr` such that `curr->code >= code`, but have yet to lock them, any other thread could `remove(pred)` before we perform the lock. This is the whole point of validation...

## 112:

Explain why we need to do the `pred->next == curr` check in our lazy lock `validate()` function:

This forgets the critical case where a new node has been inserted between the two. If, before we lock `pred,curr`, another thread inserts a new node `new_node` after `pred`, then `validate` SHOULD fail: our notion of `pred, curr` is not correct. But this change does not mark any values: it just changes `pred->next`!

## 113:


Rest of exercises are trivial

