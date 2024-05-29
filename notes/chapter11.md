# Concurrent Stacks and Elimination

## Lock Free Stack implementation

```cpp
template<typename T>
struct Node
{
    T item;
    // not atomic!
    Node<T>* prev;
};
template<typename T>
struct LockFreeStack
{
    std::atomic<Node<T>*> top; 

    bool try_push(Node<T>* node)
    {
        Node oldTop = top.load();
        node->prev = oldTop;
        return top.compare_exchange_strong(oldTop, node);
    }

    template<typename ... Args>
    void push(Args&&... args)
    {
        Node<T>* new_node = new Node<T>{std::forward<Args>(args)...};

        while(!try_push(node))
        {
            // done in thread local way
            backoff();
        }
    }

    std::optional<T> try_pop()
    {
        Node<T>* curr_top = top.load();
        Node<T>* new_top = curr_top->prev;

        if(top.compare_exchange_strong(curr_top, new_top))
        {
            auto res = std::make_optional<T>(std::move(curr_top->item));
            reclaim(curr_top);
            return res;
        }
        else
        {
            return std::nullopt;
        }
    }

    T pop()
    {
        std::optional<T> res = nullopt;
        while(!(res = try_pop()).has_value())
        {
            backoff();
        }
        return *res;
    }


};

```

That said, this scales relatively poorly: not so much because the top is a source of contention, but because of `sequential bottleneck`: calls can only proceed one after another, ordered by a successful compare and set operation.

We can never have two successful push and pop calls that occur concurrently... The critical observation here is that if a `push` is immediately followed by a `pop`, the two operations cancel out, and one can avoid fighting with the single shared atomic value entirely.

We can use an array that push calls will write to and pop calls will read from. Now, between pushes and pops, we can first try a random index of this array before backing off! Clever idea.

We need to build a sort of exchanger object that will allow threads to exchange values... 
