# Linked List: the Role of Locking

So great, now armed with a lock we can implement anything. Not quite that simple - this will slow down any system at scale that will now have every thread interacting with this object waiting for everything. So we would like to allow multiple thraeds to access the same object at the same time:

1. Fine grain synchronization: lock smaller components of the data structure
2. Optimistic synchronization: many objects will have an action start with a find - so find a smaller subnode, and then lock. But this is optimistic because if the object has changed since our lock we need to start again.
3. Lazy synchronization: split the desired action into two to postpone hard work
4. Nonblocking synchronization: `CAS` chad

We will in the chapter apply these techiques to implement a `set` using a linked list of nodes.

```cpp
struct Node
{
    T item;
    int key;
    Node* next;
};
```

We also use sentinel nodes `Head` and `Tail` that never hold nodes.

The baseline implementation is just a linked list that we lock a lock before each method. Not worth copying down.

## Fine grain synchronization

We shall first try to add concurrency by locking only the nodes that we are currently traversing, rather than locking the entire list, to allow multiple threads to access the list at the same time.

Of course, it would be an issue if we were poised to lock some node `b` and another node did something to it in the meantime (i.e. removed it). So we adopt the following policy: that we won't try to acquire a lock before having its successor, then we know that the lock itself cannot be accessed. Except for the sentinel node, only acquire the locks in such a sequential fashion.

Here is my full implementation for this design, with the use of `std::unique_lock`

```cpp
#include <memory>
#include <mutex>
#include <future>

using mutex_lock = std::unique_lock<std::mutex>;

template<typename T>
struct node
{
    node(T data, int hash_code, std::shared_ptr<node> next)
        : data(data), code(hash_code), next(std::move(next)), lock{}
    {}
    T data;
    int code;
    std::shared_ptr<node> next;
    std::mutex lock;
};

template<typename T>
struct set
{
    std::shared_ptr<node<T>> tail;
    std::shared_ptr<node<T>> head;
    std::hash<T> hasher;
    set(T s_val = T{}) :
        tail(std::make_shared<node<T>>(s_val, 1e9, nullptr)),
        head(std::make_shared<node<T>>(s_val, -1, tail)),
        hasher{}
    {}

    bool insert(T val);
    bool remove(T val);
    bool contains(T val) const;
};

template<typename T>
bool set<T>::insert(T val)
{
    int code = hasher(val);
    auto pred = head;
    mutex_lock pred_lock{pred->lock};
    auto curr = pred->next;
    mutex_lock curr_lock{curr->lock};

    while(curr->code <= code)
    {
        if(curr->code == code && curr->data == val)
        {
            return false;
        }
        pred_lock.unlock();
        pred = curr;
        curr = curr->next;
        mutex_lock next_lock{curr->lock};
        pred_lock.swap(curr_lock);
        curr_lock.swap(next_lock);
    }


    auto new_node = std::make_shared<node<T>>(val, code, curr);
    pred->next = new_node;

    return true;
}

template<typename T>
bool set<T>::remove(T val)
{
    int code = hasher(val);
    auto pred = head;
    mutex_lock pred_lock{pred->lock};
    auto curr = pred->next;
    mutex_lock curr_lock{curr->lock};

    while(curr->code <= code)
    {
        if(curr->code == code && curr->data == val)
        {
            break;
        }

        pred_lock.unlock();
        pred = curr;
        curr = curr->next;
        mutex_lock next_lock{curr->lock};
        pred_lock.swap(curr_lock);
        curr_lock.swap(next_lock);
    }

    if(curr->code == code && curr->data == val)
    {
        pred->next = curr->next;
        return true;
    }
    else
    {
        return false;
    }
}
template<typename T>
bool set<T>::contains(T val) const
{
    int code = hasher(val);
    auto pred = head;
    mutex_lock pred_lock{pred->lock};
    auto curr = pred->next;
    mutex_lock curr_lock{curr->lock};

    while(curr->code <= code)
    {
        if(curr->code == code && curr->data == val)
        {
            break;
        }

        pred_lock.unlock();
        pred = curr;
        curr = curr->next;
        mutex_lock next_lock{curr->lock};
        pred_lock.swap(curr_lock);
        curr_lock.swap(next_lock);
    }

    if(curr->code == code && curr->data == val)
    {
        return true;
    }
    else
    {
        return false;
    }
}
```

## Optimistic Synchronization

One way to reduce synchronization cost - take a chance. Search without locking, lock the node found, and then confirm that it is correct. If the found node is incorrect, start over.

In order to validate our nodes, in this case, we lock `pred, curr` and then check that A) pred points to curr, and B) pred is reachable from `head`. Note that one of our nodes could have been removed! So we need to do this.

Otherwise it is just the same code as the above, except we don't lock until we have found our desired node.

So note that this will work well when traversing twice without locking will be faster than traversing once with locking. But this does one thing very poorly: `contains()`. It runs about as quickly as `insert` or `remove`. Which isn't what we want! We expect `contains()` to get called very frequently. 

So we now introduce a way to get that:

## Lazy Synchronization

We add a field `marked` in each `node`. We now maintain the invariant that every unmarked node is reachable. The idea is that `remove` will logically mark a node as removed in one step by setting its `marked` to true, and THEN go through the work of physically removing it.

Now validate looks like this:

```cpp
bool validate(const Node* pred, const Node* curr)
{
    return !pred->marked && !curr->marked && pred->next == curr;
}
```

And so the whole implementation looks like this, operating under the assumption that we are using a perfect hashing function:

```cpp
using mutex_lock = std::unique_lock<std::mutex>;

template<typename T>
struct node
{
    node(T item, int code, std::shared_ptr<node> next = nullptr)
    : item(item), code(code), marked(false), lock{}, next(std::move(next))
    {}
    T item;
    int code;
    std::atomic<bool> marked;
    std::mutex lock;
    std::shared_ptr<node> next;
};

template<typename T>
struct set
{
    set(T filler = T{}) :
      tail(std::make_shared<node<T>>(filler, 1e8)),
      head(std::make_shared<node<T>>(filler, 0, tail)),
      hasher{}
    {}
    bool insert(T item);
    bool remove(T item);
    bool contains(T item) const;
    bool validate(const std::shared_ptr<node<T>>& pred, const std::shared_ptr<node<T>>& curr)const
    {
        return !pred->marked && !curr->marked && pred->next == curr;
    }
  private:
    std::shared_ptr<node<T>> tail;
    std::shared_ptr<node<T>> head;
    std::hash<T> hasher;
};

template<typename T>
bool set<T>::insert(T item)
{
  int code = hasher(item);
    while(true)
    {
      auto pred = head;
      auto curr = head->next;

      while(curr->code < code)
      {
          pred = curr;
          curr = curr->next;
      }

      
      std::unique_lock pred_lock{pred->lock};
      std::unique_lock curr_lock{curr->lock};

      if(!pred->marked && curr->code == code)
      {
          return false;
      }

      if(validate(pred, curr))
      {
          pred->next = std::make_shared<node<T>>(item, code, curr);
          return true;
      }

    }
    __builtin_unreachable();
}

template<typename T>
bool set<T>::remove(T item)
{
    int code = hasher(item);
    while(true)
    {
        auto pred = head;
        auto curr = pred->next;

        while(curr->code < code)
        {
            pred = curr;
            curr = curr->next;
        }

        std::unique_lock pred_lock{pred->lock};
        std::unique_lock curr_lock{curr->lock};

        // item is not in the set
        if(curr->code != code)
        {
            return false;
        }

        if(validate(pred, curr))
        {
            curr->marked = true;
            pred->next = curr->next;
            return true;
        }
    }
    __builtin_unreachable();
}
template<typename T>
bool set<T>::contains(T item) const
{
    int target_code = hasher(item);
    auto curr = head;
    while(curr != tail)
    {
        if(!curr->marked && curr->code == target_code)
        {
            return true;
        }
        curr = curr->next;
    }
    return false;
}
```

Key question: why do we need to mark the nodes first? We need so me indication to other nodes that might have acquired these nodes that something is up with them. Recall that in the prior implementation we iterated from the head up until our node to check that the path was well defined.

We say that an item is in the set if and only if it is referred to by an unmarked node.

At the time, this felt wasteful but unavoidable. After all, we need to verify the state before doing work on our node. But the only way that our state can be mangled is if a node gets removed.

Really? Can the state of a node get mangled by having stuff get inserted?

Say we acquire nodes `pred`, `curr` and are about to insert a node between them. The way that our state could get messed up would thus be if some other thread already did this operation. We don't care if `pred` is the new `next` for another node, and we don't care if `curr` got a new next node (it's still the same pointer, just with a modified contents).

And so this is why the validate function also includes a check that `pred->next == curr`. This ensures that our state hasn't been mangled by an `insert` call. 

## Non-blocking synchronization

Welcome to the holy grail. No more locks.

So a naive approach to this would just be to CAS the next field. When we want to add a node between `pred` and `curr`, we correctly build up its state (`node->next = curr`), and then perform a `pred->next.compare_exchange(curr, node)`. Similarly, when we want to remove a node `curr`, we would do `pred->next.compare_exchange(curr, curr->next)` and spin until that succeeds.

But the problem with this is that a CAS can succeed without the operation having succeeded. What do I mean by this? The text offers the following example.

Say we have a node `a` that is about to get deleted, and a node `b` that is about to be added after the node `a`. So thread `A` successfully performs its CAS to the pred of node `a`, but then thread `B` has acquired the node `a` to add to, and has no indication that any thing is wrong with it. The `next` field appears intact, so thread `B` performs `a.compare_exchange(a, b)`, which of course succeeds: in removing `A` we haven't modified it, and then `b` isn't inserted.

So the solution is that we need some way to avoid working on messed up nodes. That is, we want to use the same marked strategy as before. The nodes `next` and `marked` flags should be a single logical unit. Any attempt to set `next` for a node that is marked should fail.

We'll later delve into how to efficiently implement this in C++, but for now just assume that we can do something like this:

marked_value.CAS(T old_value, T new_value, bool old_flag, bool new_flag) where this operation will only succeed if old_value and old_flag match the state of marked value, and then the new state of marked_value is set to `{new_value, new_flag}`

So `remove` just sets the marked bit to true. And then the job of physically removing the bit is delegated to the rest of the threads traversing the list. If a thread encounters another thread that is marked as `true`, it will `compareAndSet` any new bits that it encounters.

```cpp
bool marked = curr.marked;
node* next = curr.next;
node* next_next = curr.next.next;
curr.next.CAS(marked, next, false, next_next)
```

Why do we make this change as compared to before? (Where it was only the job of `remove` to do this work)?

Because otherwise everybody has to wait for one node. Now rather than idly spinning the other nodes can do something.

Also note that we now mark the successor of a node to mark if it is marked. Why? Because ultimately we do the atomic operation on the next value of a given node...

We set `pred.next` to a new value. So the CAS will get called on `pred.next`, and so there we need to have the flag that we can compare to (we don't want to change `pred.next` if `pred` has been marked).

Check the `impl` folder to see how this gets done in `cpp`. Bit of a doozy...
