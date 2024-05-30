# Counting, Sorting, and Distributed Computing

## Shared Counting:

Is it possible to build a counter that is faster for `n` threads to increment that for one thread to increment `n` times?

### CombiningTree

Binary tree of nodes that store bookkeeping information. The actual counters value is stored at the root. Each thread gets a leaf, and at most two threads share a given leaf. To increment counter, a thread works its way up from the leaf to the root. If two threads reach the same node at the same time, their increments get joined into two. One is active and proceeds to deliver these two increments, one is passive and waits for the signal from the other. Specifically, once a node reaches the root, it traverses back down the tree to all of the passive nodes and delivers them their values.

The promise of this is higher throughput. Using a queue lock, `p` `getAndIncrement()` calls take `O(p)` time, where in ideal conditions for this tree (where all threads move up the tree together), it will take `O(logp)` time. We want to benefit throughput, not latency.

Here is a more complete description of the algorithm:

1. Proceed up the tree until we hit either the root or a node that some other thread has already been to
2. Proceed back down the tree, locking all the nodes and recording the number of nodes that we are the active node for
3. Report to our top node the number of nodes below us that we have and record the response (that is, the counter response)
4. Go back down the nodes that we have as dependencies, decrementing the recorded counter with each one
5. After decrementing the counter for each of these nodes, return the final remaining value of the counter.

Each tree node has a state that is one of the following:

```cpp
enum class CStatus
{ FIRST, SECOND, RESULT, IDLE, ROOT };
```

Which hold the following meanings:

1. First: one active thread has visited this node
2. Second: a second thread has visited the node and stored a value in the nodes `value` field to be ?combined? with the active threads value, but this is not yet complete.
3. Result: Both threads operations have been completed and the second threads result has been stored in the `value` entry
4. Root: the root node


This offers the following definition:

```cpp
struct Node
{
    CStatus status = CStatus::IDLE;
    bool locked = false;
    std::mutex mtx{};
    int firstValue, secondValue;
    int result;
    Node* parent;
    Node(Node* myParent) : parent(myParent)
    {}

    bool precombine();
}

/*
    Use an array based binary tree with the following scheme: the children of node i are 2*i and 2*i + 1

    Therefore the parent of node k is k/2
*/
struct CombiningTree
{
    CombiningTree(size_t num_nodes)
    {
        assert(is_power_of_two(num_nodes));
        nodes.reserve(num_nodes);
        leaves.reserve(num_nodes / 2);

        // root node
        nodes.emplace_back(nullptr);
        nodes[0].status = CStatus::root;

        for(size_t node = 2; node <= num_nodes; node++)
        {
            Node* parent = &nodes[(node / 2) - 1];
            nodes.emplace_back(parent);
        }

        const size_t num_leaves = num_nodes / 2;
        for(size_t leaf_node = 0; leaf_node < num_leaves; leaf_node++)
        {
            leaves.emplace_back(&nodes[num_nodes - leaf_node - 1]);
        }
    }
    int getAndIncrement(size_t thread_id);
    std::vector<Node> nodes;
    std::vector<Node*> leaves;
};

int CombiningTree::getAndIncrement(size_t thread_id)
{
    Node* my_leaf = leaves[thread_id/2];
    Node* node = my_leaf;

    // precombining phase
    while(node.precombine())
    {
        node = node->parent;
    }
    Node* stop = node;
}

bool Node::precombine()
{
    std::unique_lock<std::mutex> lk{mtx};

    while(locked)
    {
        std::this_thread::yield();
    }

    switch(status)
    {
        case CStatus::IDLE:
        {
            status = CStatus::FIRST;
            return true;
        }
        case CStatus::FIRST:
        {
            locked = true;
            status = CStatus::SECOND; 
            return false;
        }
        case CStatus::ROOT
        {
            return false;
        }
        default:
        {
            std::cerr << std::format("unexpected node state: {}", status);
            break; 
        }
    }
}
```



