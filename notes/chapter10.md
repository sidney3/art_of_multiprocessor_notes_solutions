# Concurrent Queues and the ABA Problem

A pool is a class that one can add to and remove from. Allows duplicates and does not contain a `contains()` method. There are a couple different ways that we can describle the methods on such a class:

1. _Total_: defined for all states. I.E. if our pool is bounded and we add an item, it will just throw an error / return false
2. _Partial_: defined for some states, and will wait in some states
3. _Synchronous_: defined for states where there is another thread overlapping the call: i.e. will wait until another thread adds an object WHILE WE ARE WAITING. This can be used to have a meeting between threads.

## Bounded Queue Class:

The first (lock based) bounded queue class is pretty simple. We have locks for dequeing and enqueing, and the respective action first locks this respective lock. Dequeing modifies a `head` object, while enqueuing modifies a `tail` object. And these will never be the same object as the `deque` method will wait on an atomic size variable until it is nonzero.

We also use Condition Variables that get signalled by threads once they change the state from something critical to non-critical: i.e. we are adding a node to an empty queue, or removing a node from a queue that is at capacity.

One drawback of this approach is the shared atomic counter that will lead to contention between threads that we don't necessarily need to contend. A sick approach is to have two distinct integer fields `enqSideSize` and `deqSideSize`. When a thread reaches capacity, it locks the OTHER lock and adds the other size (that will have sign negative its own) to itself.

## Unbounded total queue:

Even simpler algorithm than the above because we don't need condition variables: literally just two locks and a `head` and `tail` object.

That said, as we can potentially be accessing the same item at the same time from the `head` and the `tail`, we need to use atomics...

```cpp
template<typename T>
struct Node
{
    Node(T value, Node<T>* next = nullptr)
        : value{value}, next{next}
    {}
    T value;
    std::atomic<Node<T>*> next;
};

template<typename T>
struct MRMWQueue
{
    MRMWQueue(T init = T{})
        : tail{new Node<T>{init}},
        head{tail},
        deqLock{},
        enqLock{}
    {}
    void enq(T value)
    {
        std::unique_lock<std::mutex> lk{enqLock};

        auto node = new Node<T>{value};
        tail->next.store(node, std::memory_order_load);
        tail = node;

    }
    std::optional<T> deq()
    {
        std::unique_lock<std::mutex> lk{deqLock};

        auto result_node = head->next.load(std::memory_order_relaxed);
        
        if(result_node == nullptr)
        {
            return std::nullopt;
        }

        T res = result_node->value;

        auto to_free = head;
        head = result_node;
        delete to_free;
        
        return res;
    }
    ~MRMWQueue()
    {
        auto curr = head;
        while(curr)
        {
            auto next_curr = curr->next.load();
            delete curr;
            curr = next_curr;
        }
    }
private:
    Node<T>* tail;
    Node<T>* head;
    std::mutex deqLock;
    std::mutex enqLock;
};
```

Why do we need atomics? For the case of `tail == head`, we can get a scenario where we perform both a read and a write of the `next` value of this pointer at the same time (this would be if we call `enq` and `deq` at the same time).

As this is undefined behavior in CPP, this unfortunately means that we need to use atomics.

## Unbounded Lock Free queue

The good stuff. Here is what the implementation looks like, in cpp

```cpp
template<typename T>
struct Node
{
    Node(T val, Node* next = nullptr)
    : value(val), next(next)
    {}
    T value;
    std::atomic<Node*> next;
};

template<typename T>
struct UnboundedQueue
{
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    void enq(T val);
    T deq(void);
};

template<typename T>
void UnboundedQueue::enq(T val)
{
    Node* node = new Node(val);

    while(true)
    {
        Node* last = tail.load();
        Node* next = last->next.load();

        if(last == tail.load())
        {
            if(next == nullptr)
            {
                if(last->next.compare_exchange_weak(next, node))
                {
                    tail.compare_exchange_weak(last, node);
                    return;
                }
            }
            else
            {
                tail.compare_exchange_weak(last, next);
            }
        }
    }
}

template<typename T>
std::optional<T> UnboundedQueue::deq(void)
{
    while(true)
    {
        Node* first = head.load();
        Node* last = tail.load();
        Node* next = first->next.load();

        if(first == head.load())
        {
            if(last == first)
            {
                if(first->next == nullptr)
                {
                    return std::nullopt;
                }
                else
                {
                    tail.compare_exchange_weak(last, next);
                }
            }
            else
            {
                if(head.compare_exchange_weak(first, next))
                {
                    T res = first->value;            
                    // UB: potentially other threads that still
                    // have a pointer to first!
                    delete first;
                    return res;
                }
            }
        }
    }
}
```

So the big question in this code:

why do we have the two blocks

```
if(last == tail.load())

if(first == head.load())
```

Hypothesis: these are there for efficiency. Let's evaluate each possible branch assuming that `last != tail.load()` (a possible outcome if we don't perform this check).

Case 1: `next != nullptr`

And so we run `tail.compare_exchange(last, next)`, which fails.

Case 2: `next == nullptr`
    Case 2.1: `last->next == nullptr`
        And then this ends in us adding the new node
        as the next value.

So how could it be that this is not the tail, and what would be the issue with that? But we have assumed that `next == nullptr`, so we KNOW that no other node could have been added.

And the `tail` will never get popped by a `deq` call as it is a sentinel node. So we can conclude that this serves no purpose.

We have the following invariants: 

The `head` node never moves past the `tail` node.

To have this be the case we would need that `head == tail` and that `head == first` (to execute the CAS). But as we load `last` after loading `first`, this would mean that `last == tail`, and so we would enter the first `if` statement and not move `head` past `tail`.

if a node was ever the `tail` node and its next value is `nullptr`, then it is currently the `tail` node.

We never set the next value of a node to `nullptr`. So for a node to lose its status as the tail we would first need to add another node after it, setting its `next` value to not `nullptr`. Given this, let's break down our cases.

Assume that `last != tail.load()`

Case 1: `next == nullptr` and by our second invariant we have that our assumption must be false.

Case 2: `next != nullptr`. Then we perform `tail.compare_exchange_weak(last, next)`, which will do nothing because `tail != last`

Now assume that `first != head.load()`

Case 1: `last == first`
    Case 1.1: `first->next == nullptr`.
    So `last == first`, and therefore `first` was once equal to `tail`. And therefore by our second invariant we have that `first == tail` and so, by our second invariant, as our `head` never moves past tail, we must have that `first == head` and so our assumption is false.
    Case 1.2: `first->next != nullptr`
        Case 1.2.1: `first != tail`: and so `tail.compare_exchange_weak(last, next)` does nothing.
        Case 1.2.2: `first == tail`: and therefore `first == head`, as the head can only move forward, but cannot move forward past the tail

Case 2: `last != first`:
    And we will perform `head.compare_exchange_weak(first, next)`, which will do nothing because `head != last`

So that silly if statement is not necessary. 

But this is part of a bigger idea of VALIDATE and ACT. I.E. if we have some piece of data that is dependent on another piece of data, we want to verify that this OTHER piece of data is correct in the state of the program.

I'm a little sussed out by this. But whatever.


### We now move onto the question of recycling nodes

We might want to use a local free list (local to each thread). If we are just directly reusing the pointers, this could fail in the following way:

We have one thread that is trying to `deq(a)` onto a queue with just two elements, `a` and `b`. It is about to execute the instruction `head.compare_exchange(a, b)`, where `b` is the node that it has identified as the next node after `a`. Then, another thread could `deque(a)`, `deque(b)`, `enque(a)`, and now the `cas` will execute to true, but it is total nonsense that the head should now be `b`. This issue arises because the `compare_exchange(a,b)` fundamentally does not test if the head is currently `a`, but if the current state is the same as when we found out that the current head was `a`, in which case we are able to perform our add. 

So if we want to reuse pointers, we need to add generations to them. That is, when a pointer gets freed and we then reuse it we increment the generation of that pointer.

## Synchronous queues

Here, the enqeuer will block until its item is taken by a dequer. As the text says: rendez-vous

And the text then presents the following deeply painful snippet:

```cpp
template<typename T>
struct SynchronousQueue
{
    std::optional<T> _item;
    bool enqueuing;
    std::condition_variable cv;
    std::mutex lock;

    void enq(T item)
    {
        std::unique_lock<std::mutex> lk{lock};

        cv.wait(lk, [this](){return !enqueuing;});

        enqueuing = true;
        _item = item;
        cv.notify_all();

        cv.wait(lk, [this]{return !_item.has_value();});
        enqueuing = false;
        cv.notify_all();
    }

    T deq(void)
    {
        std::unique_lock<std::mutex> lk{lock};

        cv.wait(lk, [this](){return _item.has_value();});

        T res = *_item;
        _item = std::nullopt;
        cv.notify_all();
        return res;
    }
};
```

### Dual Data structures

Let's try to solve this problem by splitting the method into two steps. First, we place a reservation into a single shared queue. Then, we spin on that reservation until it is fulfilled.

So at any time, the queue will contain either `enq` reservations, `deq` reservations, or is empty.

We thus have the following highly complicated data structure, where we manage an internal LL queue.

```cpp
class enum NodeType
{
    ITEM, RESERVATION
};

template<typename T>
struct Node
{
    Node(NodeType type, T* item, Node* next = nullptr)
    : type(type), next(next), item(item)
    {}

    const NodeType type;
    std::atomic<Node*> next;
    std::atomic<T*> my_item;
};

struct SynchronousDualQueue
{
    std::atomic<Node<T>> head;
    std::atomic<Node<T>> tail;

    void enq(T e);
};

template<typename T>
void SynchronousDualQueue::enq(T e)
{
    auto node = new Node<T>{NodeType::ITEM, &e}

    while(true)
    {
        auto t = tail.load(), h = head.load();

        // add to queue and wait
        if(h == t || t.type == NodeType::ITEM)
        {
            auto n = t->next.load();

            if(t != tail.load())
            {
                continue;
            }

            if(n != nullptr)
            {
                // help the tail along
                // just like in the prior queue
                tail.compare_exchange_weak(t, n);
                continue;
            }

            // set tail->next to our new node
            if(tail->next.compare_exchange_weak(n, node))
            {

                tail.compare_exchange_weak(t, node);

                while(node->my_item.load() != nullptr);

                h = head.load();

                // recall: head is a sentinel node
                if(node == h->next.load())
                {
                    // help out
                    // not necessary
                    head.compare_exchange_weak(h, node);
                }

                return;
            }
        }
        else
        {
            Node* n = h->next.load();
            if(t != tail.get() || h != h.get() || n == nullptr)
            {
                continue;
            }

            // if success: we delivered our item
            bool success = n->item.compare_exchange_weak(nullptr, e);
            
            // either way, head is now not nullptr so can be moved up
            head.compare_exchange_weak(h, n);

            /*
    So critically, a node gets popped from the queue iff it gets its request fulfilled.
            */

            if(success)
            {
                return;
            }
        }
    }
}

```
