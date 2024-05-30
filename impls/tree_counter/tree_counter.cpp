#include <vector>
#include <mutex>
#include <cassert>
#include <iostream>
#include <thread>
#include <format>

bool debug = false;

template<typename Printable>
void maybe_print(Printable&& p)
{
    if(debug)
    {
        std::cout << p;
    }
}

/*
0010101000 -> 001000000
*/
int top_mask(int x)
{
    int res = 0;
    for(int y = 1;
        y <= x;
        y <<= 1)
    {
        if(y & x)
        {
            res = y;
        }
    }
    return res;
}
/*
   Rounds an integer up to 2^i such that i is minimized and
   2^i <= x
*/
int round_up_2pow(int x)
{
    int top = top_mask(x);

    if(top == x)
    {
        return top;
    }
    return (top << 1);
}
struct Node
{
    enum class NodeStates
    {
        IDLE, FIRST, SECOND, RESULT, ROOT
    };

    NodeStates NState = NodeStates::IDLE;
    bool locked = false;
    int value, firstValue, secondValue;
    std::mutex mtx{};
    std::condition_variable cv{};
    Node* parent;

    Node(Node* parent) : parent(parent)
    {}
    Node() : parent(nullptr)
    {}
    bool upwardsVisit()
    {
        std::unique_lock lk{mtx};

        cv.wait(lk, [this](){return !locked;});

        maybe_print(std::format("thread {}: upwards Visit to state {}\n", std::this_thread::get_id(), NodeStateString(NState)));

        switch(NState)
        {
            case NodeStates::IDLE:
            {
                NState = NodeStates::FIRST;
                return true;
            }
            case NodeStates::FIRST:
            {
                NState = NodeStates::SECOND;
                locked = true;
                return false;
            }
            case NodeStates::ROOT:
            {
                return false;
            }
            default:
            {
                std::cerr << std::format("unrecognized state from thread {} in upwardsVisit: {}\n", std::this_thread::get_id(), NodeStateString(NState));
                throw std::exception{};
            }
        }
    }
    /*
       Two cases:
       Case 1: NodeState == FIRST (thus no node came to this node) and
       we return 0

       Case 2: NodeState == SECOND (thus a node came to this spot) and
       so we are now interested in the number of nodes that this node
       is an active node for.

       No - we wait until the node is no longer locked.
       How do we prevent other nodes from joining? I guess the children
       should all be locked?
    */
    int accumulate(int dependencyCount)
    {
        std::unique_lock lk{mtx};

        cv.wait(lk, [this](){return !locked;});

        locked = true;
        firstValue = dependencyCount;
        
        switch(NState)
        {
            case NodeStates::FIRST:
            {
                return firstValue;
            }
            case NodeStates::SECOND:
            {
                return firstValue + secondValue;
            }
            default:
            {
                std::cerr << std::format("unrecognized state from thread {} in accumulate: {}\n", std::this_thread::get_id(), NodeStateString(NState));
                throw std::exception{};
            }
        }
    }
    /*
        We know the value of the counter that is to be distributed
        to this value.
    */
    void storeResult(int global_result)
    {
        std::unique_lock lk{mtx};
        maybe_print(std::format("STORE RESULT CALL: state from thread {} in storeResult {}\n", std::this_thread::get_id(), NodeStateString(NState)));

        switch(NState)
        {
            case NodeStates::FIRST:
            {
                locked = false;
                NState = NodeStates::IDLE;
                cv.notify_all();
                return;
            }
            case NodeStates::SECOND:
            {
                value = global_result + firstValue;
                maybe_print(std::format("thread {}: handed off result value {}\n", std::this_thread::get_id(), value));
                NState = NodeStates::RESULT;
                cv.notify_all();
                return;
            }
            default:
            {
                std::cerr << std::format("unrecognized state from thread {} in storeResult {}\n", std::this_thread::get_id(), NodeStateString(NState));
                throw std::exception{};
            }
        }
    }
    int op(size_t dependencyCount)
    {
        std::unique_lock lk{mtx};

        switch(NState)
        {
            case NodeStates::ROOT:
            {
                int res = value;
                value += dependencyCount;
                return res;
            }
            case NodeStates::SECOND:
            {
                // set count of waiting nodes
                secondValue = dependencyCount;

                // this should release the threads that have this
                // as a dependency to relock it and set result
                locked = false;
                cv.notify_all();

                // wait for delivery of value from parent
                cv.wait(lk, [this](){return NState == NodeStates::RESULT;});

                maybe_print(std::format("thread {}: got result delivered", std::this_thread::get_id()));
                NState = NodeStates::IDLE;
                locked = false;
                cv.notify_all();
                return value;
            }
            default:
            {
                std::cerr << std::format("unrecognized state from thread {} in op(){}\n", std::this_thread::get_id(), NodeStateString(NState));
                throw std::exception{};
            }

        }
    }
    
    bool is_locked()
    {
        return locked;
    }

    std::string_view NodeStateString(NodeStates state)
    {
        switch(state)
        {
            case NodeStates::RESULT:
            {
                return "result";
            }
            case NodeStates::FIRST:
            {
                return "first";
            }
            case NodeStates::SECOND:
            {
                return "second";
            }
            case NodeStates::IDLE:
            {
                return "idle";
            }
            case NodeStates::ROOT:
            {
                return "root";
            }
        }
    }

};

class TreeCounter
{
public:
    /*
       If we have N threads that we need to count for,
       we round this up to a power of 2 (call this figure
       2^i) and will thus need 2^(i-1) leaves. This means

       2^{i-1} + 2^{i-2} + ... + 2^0 total nodes
       = 2^i - 1
    */
    TreeCounter(size_t num_threads)
        : nodes(round_up_2pow(num_threads) - 1), 
        leaves(round_up_2pow(num_threads) >> 1)
    {
        assert(num_threads > 1 &&
                "Need more than one thread");
        // root node
        nodes[0].NState = Node::NodeStates::ROOT;

        /*
            Children of node i are 2*i and 2*i + 1 so parent of node
            j is j/2
        */
        for(size_t node = 2; node <= nodes.size(); node++)
        {
            nodes[node - 1].parent = &nodes[(node/2) - 1];
        }

        /*
            Assign arbitrary leaf numbers. Doesn't matter which thread
            gets which leaf.
        */
        for(size_t leaf = 0; leaf < leaves.size(); leaf++)
        {
            maybe_print(std::format("leaf index: {}\n", nodes.size() - leaf - 1));
            leaves[leaf] = &nodes[nodes.size() - leaf - 1];
        }

    }
    int getAndIncrement(size_t thread_id)
    {
        std::vector<Node*> dependencies;
        Node* leafNode = leaves[thread_id / 2];
        Node* node = leafNode;

        // traverse up until stopped
        // at this point ONLY the top node is locked
        while(node->upwardsVisit())
        {
            node = node->parent;
            assert(node != nullptr);
        }

        // traverse up the tree again, locking the nodes
        // and gathering a count of the waiting children
        // WE are the active node
        Node* last = node;
        node = leafNode;
        int childrenCount = 1;

        while(node != last) 
        {
            childrenCount = node->accumulate(childrenCount);
            dependencies.push_back(node);
            node = node->parent;
        }
        maybe_print(std::format("thread {}: {} dependencies\n", std::this_thread::get_id(), childrenCount));


        // at this point ALL the nodes INCLUDING last are locked
        for(const auto node_ptr : dependencies)
        {
            assert(node_ptr->is_locked());
        }

        // set the count of waiting nodes for the top (active)
        // node for which we are the passive node, and
        // await the result

        // once we get the value from res, the node is free!
        int res = last->op(childrenCount);
        maybe_print(std::format("thread {}: got result {}\n", std::this_thread::get_id(), res));

        // all dependencies are locked at this point, top node is not
        // need to unlock in top down order

        // if the node state is NodeStates::SECOND there is a second
        // path going upwards that is still locked. So we prevent
        // everybody from going upwards

        while(!dependencies.empty())
        {
            Node* highestNode = dependencies.back();
            assert(highestNode != last);
            dependencies.pop_back();

            highestNode->storeResult(res);
        }
        return res;
    }
private:
    std::vector<Node> nodes;
    std::vector<Node*> leaves;
};

void inc_thread(TreeCounter& tc, size_t tid)
{
    maybe_print(std::format("thread_id: {}, idx: {}\n", std::this_thread::get_id(), tid));
    for(size_t i = 0; i < 1000; ++i)
    {
        int res = tc.getAndIncrement(tid);
        std::cout << std::format("{}\n", res);
        maybe_print(std::format("COUNTER RESPONSE: thread: {}, value: {}\n", std::this_thread::get_id(), res));
    }
}

int main()
{
    static constexpr int num_threads = 10;

    TreeCounter tc{num_threads};

    std::array<std::thread, num_threads> counters;

    for(size_t i = 0; i < num_threads; ++i)
    {
        counters[i] = std::thread{inc_thread, std::ref(tc), i};
    }

    for(size_t i = 0; i < num_threads; ++i)
    {
        counters[i].join();
    }
}
