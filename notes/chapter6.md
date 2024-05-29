# Universality of Consensus

Def: We call an object universal if it has concensus number at least `n` for an `n` thread system and equivalently, any concurrent object can be implemented using it.


---------------------
|CNumber  |Objects  |
---------------------
|1        |Atomics  |
---------------------
|2        |GetAndSet(), Queue, Stack, GetAndAdd()
---------------------
|m        |(m, m choose 2) register set
---------------------
|         |         |
---------------------
|infty    |CAS, LL/SC, memory-to-memory move
---------------------

We now present a lock free implementation of *any* discrete object (meaning that given an object state and some invocation to the object, there is exactly one resulting state). Say that we have `N` threads in the system. Our idea will be to represent such an object as a valid initial state and a log of state updates. These state updates will be stored as a linked list.

When two threads concurrenty call `update()`, we use a consensus protocol to decide which will get their update reflected. Then, the winning thread goes through the log of updates, from tail to the new object, and applies each update to the initial state.

```cpp
template<typename T>
struct Consensus;

struct Node
{
    std::atomic<int> sequence;
    std::atomic<Node*> next;
    Consensus<Node*> decideNext;
    Invoc invoc;

    Node(Invoc invoc) : invoc(invoc), sequence(0), decideNext(), next(nullptr)
    {}
    Node(int seq) : sequence(seq), decideNext(), next(nullptr)
    {}

    template<long N>
    static Node* max(const std::array<Node, N>& arr)
    {
        static_assert(N > 0);
        Node* res = &arr[0];
        for(size_t i = 1; i < N; i++)
        {
            if(arr[i].sequence > res->sequence)
            {
                res = &arr[i];
            }
        }
        return res;
    }
};
```

```cpp
template<long N>
struct LFUniversal
{
    Node tail;
    std::array<Node, N> nodes;
    LFUniversal() : LFUniversal(std::make_index_sequence<N>())
    {}

    Response apply(Invoc invoc, size_t curr_thread)
    {
        Node preferred{ invoc };

        while(preferred.sequence == 0)
        {
            Node* before = Node::max(nodes);
            Node* after = before.decideNext.decide(&preferred);
            before.next = after;
            after.sequence = before.sequence + 1;
            head[curr_thread] = after;
        }

        Node* curr = tail.next;
        while(curr != &preferred)
        {
            curr->invoc.apply();
            curr = curr->next;
        }
        return curr->invoc.apply();
    }

    template<typename ... Is>
    LFUniversal(std::index_sequence<Is...>)
    : tail(1), nodes{ {static_cast<void>(Is), tail}... }
    {}
};
```

Beautiful...

Here is the implementation in pseudocode
```cpp
struct Node
{
    Node* next;
    Consensus<Node> decideNext;
    Invoc invoc;
    int index; 
    Node() : index(0)
    {}
};

// N = the number of threads
template<long N> 
struct UniversalLockFreeObject
{
    std::array<Node, N> head;
    Node tail; // initialized with index = 1

    // assume thread_id is a member of {0, 1, ...., N-1}
    Result apply(Invoc invoc, size_t thread_id)
    {
        Node preferred{ invoc };

        while(preferred.index == 0)
        {
            Node after = head[thread_id].decideNext.decide(preferred);
            head[thread_id].next = after;
            after.index = head[thread_id].index + 1;
            head[thread_id] = after;
        }

        Node curr = tail.next;
        while(curr != preferred)
        {
            curr.invoc.apply();
        }
        return curr.invoc.apply();
    }
};
```

## A wait free universal construction


Typo on page 131: no variable `prefer` is defined

We now present a wait free implementation, which uses a similar idea but now we allow threads to help other threads out, which prevents a thread from getting overtaken an arbitrary number of times...

```cpp
struct WFUniversal
{
    std::array<Node,N> announce;
    std::array<Node,N> head;
    Node tail;

    Result apply(Invoc invoc, size_t i)
    {
        announce[i]{ invoc };
        head[i] = Node::max(head);
        head[i].increaseRefCount();

        while(announce[i].sequence == 0)
        {
            Node before = head[i];
            Node help = announce[(before.sequence + 1) % N];
            Node prefer = (help.sequence == 0) ? help : announce[i];
            after = before.decideNext.decide(prefer);
            before.next = after;
            after.sequence = before.sequence + 1;
            head[i].decreaseRefCount();
            head[i] = after;
            head[i].increaseRefCount();
        }

        // same code as before to replay the state updates from tail -> announce[i]

    }
};
```

The logic here is simple: we will usually default to proposing our own value, but will sometimes be generous and help another thread.

The critical thing to understand is that no duplicate nodes will get added:

The only way for this to happen would be for one to complete a cycle if the while loop and then have the other append to this newly added node. But that's impossible: we can never enter the while loop / add for a value with a non-zero sequence.


Proof: no thread will starve. Each time we successfully complete a loop we increment head's sequence by 1. We add elements one at a time so there aren't any values that are going to get skipped. So we are just going to iterate up and up...

That said, the text wants to use this portion to introduce us to some new proof techniques, so we won't fight it too much...

Say `max(head[])` is the node with the largest sequence number in `head`, `c in head[]` is the assertion that `c` has been assigned to some value in `head[]`.

We also have a notion of "ghost variables" - one that does not appear explicitly in the program, nor alters the behavior in any way, but can help us reason about the program.

Let us use the following variables:

```
concur(A) // set of nodes that have been stored in the head[] array since the last announcement of threadA
start(A) // the sequence number of max(head[]) when threadA last announced
```

So appending our code with these variables, we get as follows:
```cpp
struct WFUniversal
{
    std::array<Node,N> announce;
    std::array<Node,N> head;
    Node tail;

    Result apply(Invoc invoc, size_t i)
    {
        announce[i]{ invoc };
        head[i] = Node::max(head);
        <start[i] = head[i]>;
        <concur[i] = {}>;

        while(announce[i].sequence == 0)
        {
            Node before = head[i];
            Node help = announce[(before.sequence + 1) % N];
            Node prefer = (help.sequence == 0) ? help : announce[i];
            after = before.decideNext.decide(prefer);
            before.next = after;
            after.sequence = before.sequence + 1;
            head[i] = after;
            <for each j, concur[j] = concur[j] + after>;
        }

        // same code as before to replay the state updates from tail -> announce[i]
        head[i] = announce[i]; 
        < for each j, concur[j] = concur[j] + after>;

    }
};
```

Note that the following is an invariant:

```
|concur(A)| + start(A) = max(head[])
```
because each time `concur(A)` grows by one, so does `max(head[])`

Lemma: `|concur(A)| > n => announce[A] in head`
Proof: if the above is true, then we have successive nodes `b,c` with sequence numbers `A, A-1`



