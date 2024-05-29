#include <memory>
#include <iostream>
#include <cassert>
#include <optional>

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
        tail->next.store(node);
        tail = node;

    }
    std::optional<T> deq()
    {
        std::unique_lock<std::mutex> lk{deqLock};

        auto result_node = head->next.load();
        
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

int main()
{
    MRMWQueue<int> Q;
    Q.enq(5);
    assert(*Q.deq() == 5);

    Q.enq(6);
    Q.enq(9);

    assert(*Q.deq() == 6);
    assert(*Q.deq() == 9);
    assert(!Q.deq().has_value());

    Q.enq(6);
    Q.enq(6);
    Q.enq(6);
    Q.enq(6);
    Q.enq(6);
    Q.enq(6);
}
