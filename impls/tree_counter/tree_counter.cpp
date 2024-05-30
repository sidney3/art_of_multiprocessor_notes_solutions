#include <vector>

struct Node
{
    enum class NodeStates
    {
        IDLE, FIRST, SECOND, RESULT, ROOT
    };

    NodeStates NState;
    int first_value;
    int second_value;

};

int main()
{

}
