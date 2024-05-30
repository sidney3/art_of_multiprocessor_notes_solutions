#include <vector>
#include <mutex>
#include <cassert>

bool is_bin_tree_count(int x)
{
    return !(x & (x+1));
}

struct Node
{
    enum class NodeStates
    {
        IDLE, FIRST, SECOND, RESULT, ROOT
    };

    NodeStates NState = NodeStates::IDLE;
    bool locked = false;
    int value = 0;
    int firstValue;
    int secondValue;
    std::mutex mtx{};
    std::condition_variable cv{};
    Node* parent;

    Node(Node* parent) : parent(parent)
    {}
    Node() : parent(nullptr)
    {}
};

class TreeCounter
{
public:
    /*
        1 + 2 + 4 + 8
    */
    TreeCounter(size_t num_nodes)
        : nodes(num_nodes), leaves((num_nodes+1) / 2)
    {

    }
    int getAndIncrement(size_t thread_id)
    {
        return 0;
    }
private:
    std::vector<Node> nodes;
    std::vector<Node*> leaves;
};

int main()
{

}
