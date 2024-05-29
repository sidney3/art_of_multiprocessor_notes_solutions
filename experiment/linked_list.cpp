#include <cassert>
#include <memory>
#include <mutex>
#include <future>

using mutex_lock = std::unique_lock<std::mutex>;

template<typename T>
struct node
{
    node(T data, int hash_code, std::shared_ptr<node> next)
        : data(data), code(hash_code), next(std::move(next)), lock{}
    {}
    T data;
    int code;
    std::shared_ptr<node> next;
    std::mutex lock;
};

template<typename T>
struct set
{
    std::shared_ptr<node<T>> tail;
    std::shared_ptr<node<T>> head;
    std::hash<T> hasher;
    set(T s_val = T{}) :
        tail(std::make_shared<node<T>>(s_val, 1e9, nullptr)),
        head(std::make_shared<node<T>>(s_val, -1, tail)),
        hasher{}
    {}

    bool insert(T val);
    bool remove(T val);
    bool contains(T val) const;
};

template<typename T>
bool set<T>::insert(T val)
{
    int code = hasher(val);
    auto pred = head;
    mutex_lock pred_lock{pred->lock};
    auto curr = pred->next;
    mutex_lock curr_lock{curr->lock};

    while(curr->code <= code)
    {
        if(curr->code == code && curr->data == val)
        {
            return false;
        }
        pred_lock.unlock();
        pred = curr;
        curr = curr->next;
        mutex_lock next_lock{curr->lock};
        pred_lock.swap(curr_lock);
        curr_lock.swap(next_lock);
    }


    auto new_node = std::make_shared<node<T>>(val, code, curr);
    pred->next = new_node;

    return true;
}

template<typename T>
bool set<T>::remove(T val)
{
    int code = hasher(val);
    auto pred = head;
    mutex_lock pred_lock{pred->lock};
    auto curr = pred->next;
    mutex_lock curr_lock{curr->lock};

    while(curr->code <= code)
    {
        if(curr->code == code && curr->data == val)
        {
            break;
        }

        pred_lock.unlock();
        pred = curr;
        curr = curr->next;
        mutex_lock next_lock{curr->lock};
        pred_lock.swap(curr_lock);
        curr_lock.swap(next_lock);
    }

    if(curr->code == code && curr->data == val)
    {
        pred->next = curr->next;
        return true;
    }
    else
    {
        return false;
    }
}
template<typename T>
bool set<T>::contains(T val) const
{
    int code = hasher(val);
    auto pred = head;
    mutex_lock pred_lock{pred->lock};
    auto curr = pred->next;
    mutex_lock curr_lock{curr->lock};

    while(curr->code <= code)
    {
        if(curr->code == code && curr->data == val)
        {
            break;
        }

        pred_lock.unlock();
        pred = curr;
        curr = curr->next;
        mutex_lock next_lock{curr->lock};
        pred_lock.swap(curr_lock);
        curr_lock.swap(next_lock);
    }

    if(curr->code == code && curr->data == val)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int main()
{
    set<int> S;
    S.insert(5);
    assert(S.remove(5));
    S.insert(9);
    S.insert(10);
    S.insert(11);
    S.insert(2);
    assert(S.contains(9));
    assert(S.remove(9));
    assert(!S.contains(9));
    auto fut1 = std::async(std::launch::async, [&S](){
            S.insert(5);
            S.insert(5);
            S.remove(5);
            assert(!S.contains(5));
            S.insert(5);
            assert(S.contains(5));
            });
    auto fut2 = std::async(std::launch::async, [&S](){
            S.insert(6);
            assert(S.contains(6));
            S.insert(7);
            assert(S.contains(7));
            });
    auto fut3 = std::async(std::launch::async, [&S](){
            S.insert(7);
            S.insert(9);
            assert(S.remove(9));
            S.insert(13);
            S.insert(15);
            S.insert(17);
            assert(S.contains(17));
            assert(!S.contains(14));
            });
    auto fut4 = std::async(std::launch::async, [&S](){
            S.insert(8);
            });
    
    fut1.get();
    fut2.get();
    fut3.get();
    fut4.get();
}
