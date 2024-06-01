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

### Counting Networks

Say now that we have `w` counters. Counter `i` (1 indexed) will distribute unique indices modulo `w`: `i, w + i, 2*w + i,..., k*w + i...`.

Def: we say a __balancer__ is a ?thing? with two input wires and two output wires. Tokens arrive at arbitrary times and emerge on output wires, also at arbitrary times. Critically, the balancer will balance across the two wires. It can be viewed as a toggle: sends one input token to the top wire, the next to the bottom, and so on...

Has two states, `up` and `down`, which describe where it will send the next token. Let `x0` and `x1` denote the number of tokens that arrive at top and bottom input wires, and `y0`, `y1` the number of tokens that leave from the top and bottom output wires. Certainly `x0 + x1 >= y0 + y1` - we never create tokens.

We say a balancer is quiescent if `x0 + x1 = y0 + y1`. 

We can construct a __balancing network__ by connecting a balancers output wires to other balancers output wires. Certainly using this we can create a balancer of arbitrary width (though not perhaps with equal probability of output wire).

We say the __depth__ of a balancing network is the maximum number of balancers one can traverse from one end to another.. As with individual balancers, tokens are not created: `\sum x_i >= \sum y_i` And as before quiscent if these values are the same.

Note that while this is visualized as switches on a network, it can just as easily be objects in memory that hold references to other balancers.

The book then presents a very interesting network that satisfies the following property (where, recall, `x_i` is input count and `y_i` is output count for i/o wires `x_i`, `y_i`):

```
n = \sum x_i => y_i = (n / w) + (i mod w)
```

Note that the above is just wrong... The below is more correct (basically a question of if we get the overflow token or not)
```
n = \sum x_i => y_i = (n / w) + (i < (n mod w))
```

This network is specified and proven in the below python script, that is also in `notes/4_bitonic_proof.py`

```python
import math
class TerminalWire:
    def __init__(self, name):
        self.count = 0
        self.name = name
    def pushToken(self):
        self.count += 1
        # print(f'Balancer {self.name} received a token')

class IntermediateBalancer:
    def __init__(self, upConnection, downConnection):
        self.upConnection = upConnection
        self.downConnection = downConnection
        self.switchUp = True

    def pushToken(self):
        if self.switchUp:
            self.upConnection.pushToken()
            self.switchUp = False
        else:
            self.downConnection.pushToken()
            self.switchUp = True 
class SourceWire:
    def __init__(self, Balancer):
        self.balancer = Balancer
    def pushToken(self):
        self.balancer.pushToken()


class Bitonic4:
    def __init__(self):

        self.terminals = [TerminalWire("1"), TerminalWire("2"), TerminalWire("3"), TerminalWire("4")]

        self.i5, self.i6 = IntermediateBalancer(self.terminals[0], self.terminals[1]), IntermediateBalancer(self.terminals[2], self.terminals[3])
        self.i3, self.i4 = IntermediateBalancer(self.i5, self.i6), IntermediateBalancer(self.i5, self.i6)
        self.i1, self.i2 = IntermediateBalancer(self.i3, self.i4), IntermediateBalancer(self.i4, self.i3)
        
        self.sources = [SourceWire(self.i1), SourceWire(self.i1), SourceWire(self.i2), SourceWire(self.i2)]
    def pushToWire(self, wireIndex):
        self.sources[wireIndex].pushToken()
    def queryTerminals(self):
        res = [0] * len(self.terminals)
        for i,term in enumerate(self.terminals):
            res[i] = term.count
        return res

def allCombinations(List, CombinationSize):
    if CombinationSize == 0:
        return [[]]

    res = []
    for combo in allCombinations(List, CombinationSize - 1):
        for elt in List:
            comboCopy = combo.copy()
            comboCopy.append(elt)
            res.append(comboCopy)
    return res

def hasStepProperty(freqs):
    total_tokens = sum(freqs)
    num_wires = len(freqs)

    res = True
    for wire, wire_tokens in enumerate(freqs):
        expected_tokens = math.ceil((total_tokens - wire) / num_wires)
        res = res and wire_tokens == expected_tokens

    return res

def main():
    assert(hasStepProperty([1,1,1,1]))
    assert(hasStepProperty([1,1,1,0]))
    assert(hasStepProperty([1,1,0,0]))
    assert(hasStepProperty([1,0,0,0]))
    assert(hasStepProperty([0,0,0,0]))

    for numTokens in range(1, 9):
        for combo in allCombinations(range(4), numTokens):
            network = Bitonic4()
            for wire in combo:
                network.pushToWire(wire)
            assert(hasStepProperty(network.queryTerminals()))

if __name__ == "__main__":
    main()
```

This is called the step property, and a balancing network satisfying it is called a __counting network__, as counting can be done by giving tokens that emerge on wire `y_i` the consecutive numbers `i, i + w, i + 2*w,...`, as we expect a token to arrive to this point every `w` pushes into the system.

Makes total sense, just think about it a little bit

Lemma: let `y_0,...,y_{w-1}` be a sequence of non-negative integers. Then the following are equivalent:

1. For each `i,j, i < j`, `0 <= y_i - y_j <= 1`
2. Either `y_i = y_j` for each `i < j`, or there exists some `c` such that for any `i < c` and `j >= c`, `y_i - y_j = 1`
3. If `m = \sum y_i`, then `y_i = ceil(m - i / w)`

### Bitonic Counting Network

We shall now generalize the construction shown above...

Define a width `w` sequence of inputs `x_0,...,x_{w-1}` to be a collection of tokens partitioned into `w` subsets `x_i`.

Define a width `2k` network `Merger[2k]` as follows: two input sequences of width `k`, `x` and `x'`, and a single output sequence of width `2k`. IF both `x` and `x'` have the step property, then so does to output sequence `y` of our larger network. Define this inductively - 

when `k = 1` we can get this with a single balancer, 

Otherwise for `k = 2k'`, we consider two `Merger[k']` networks. We say that we have input sequences `{x_0,...,x_{k-1}}` and `{x'_0,...,x'_{k - 1}}`. We send all the even `x`'s and odd `x'`'s to one merger, and then the reverse for the other. We say that the outputs of these are `z`, `z'`. So we have a balancer for each such pair that takes in `z_i, z'_i` and goes to `y_{2*i}, y_{2*i + 1}`

Proof that the if `x, x'` have the step property, then so does `y`:

Proof by induction: clearly have this for `Merger[2]`. 

What about this for `2*k`? Intuitively, half of the input from each of our sources goes to each of the two sub mergers. So each input source gives about half of its load to each of the two sub-mergers. So why do we need to do the balancing at the end? So the idea between merging the evens with the odds is that the difference between the number of tokens that has gone to either of the two sub-mergers will always be between one.

But we don't know which one will receive the additional token first - and so this way if the number of tokens to both is the same, then certainly we will be counting up, but if we then receive a token that would offset us to being not the same, it always "starts" a new switch that will be pointing up. 

The name says it all here: intuitively this can be thought of as way to take two smaller networks with the balancing property and combine them into a single network with twice the input size and still has the balancing property.

Then, the `Bitonic[2k]` network by passing the outputs from two `Bitonic[k]` networks into a `Merger[2k]` network, where induction here is again that `Bitonic[2]` is just a single balancer. And we know that the step property gets preserved by the merger (that's the big goal of the merger - merge...)



