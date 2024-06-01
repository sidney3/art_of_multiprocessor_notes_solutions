#include <vector>
#include <mutex>
#include <cassert>
#include <iostream>
#include <thread>
#include <format>
#include "test_counter.h"


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

        if_debug(std::cout << std::format("thread {}: upwards Visit to state {}\n", std::this_thread::get_id(), NodeStateString(NState)));

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
        if_debug(std::cout << std::format("STORE RESULT CALL: state from thread {} in storeResult {}\n", std::this_thread::get_id(), NodeStateString(NState)));

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
                if_debug(std::cout << std::format("thread {}: handed off result value {}\n", std::this_thread::get_id(), value));
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

                /*
                This is the scariest unlock of the program:

                how do we know that another thread will not come
                from below us and get to this node, rather than what
                we want (which is for our parent to come deliver us
                the value)?

                When we call OP from some node A, the child path to that
                node is all locked.

                Likewise, there must be a parent that came from another
                node (that is also locked all the way down: we get
                this guarantee because we traverse the nodes from top
                to bottom when delivering the result)
                */
                locked = false;
                cv.notify_all();

                // wait for delivery of value from parent
                cv.wait(lk, [this](){return NState == NodeStates::RESULT;});

                if_debug(std::cout << std::format("thread {}: got result delivered", std::this_thread::get_id()));
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
            if_debug(std::cout << std::format("leaf index: {}\n", nodes.size() - leaf - 1));
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

        // for each child, we tell it the amount of nodes
        // that came before it (that are also our children)
        // so when we eventually deliver its result to it
        // (this result coming from the root node)
        // it can calculate its offset from this value

        /*
            For any children that are themselves active nodes
            to some other nodes, we need to wait for their 
            result, and so we wait until each of these children
            unlock -> as either they are

            1) only passive nodes (so we lock them directly)
            2) active nodes (so they locked themselves and will
            only unlock once done with their operation)
        */
        Node* last = node;
        node = leafNode;
        int childrenCount = 1;

        while(node != last) 
        {
            childrenCount = node->accumulate(childrenCount);
            dependencies.push_back(node);
            node = node->parent;
        }


        if_debug(std::cout << std::format("thread {}: {} dependencies\n", std::this_thread::get_id(), childrenCount));


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
        if_debug(std::cout << std::format("thread {}: got result {}\n", std::this_thread::get_id(), res));

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
    if_debug(std::cout << std::format("thread_id: {}, idx: {}\n", std::this_thread::get_id(), tid));
    for(size_t i = 0; i < 1000; ++i)
    {
        int res = tc.getAndIncrement(tid);
        std::cout << std::format("{}\n", res);
        if_debug(std::cout << std::format("COUNTER RESPONSE: thread: {}, value: {}\n", std::this_thread::get_id(), res));
    }
}

int main()
{
    test_counter<TreeCounter>();
}
