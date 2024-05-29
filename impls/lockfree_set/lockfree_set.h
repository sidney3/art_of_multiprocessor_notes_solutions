#include <atomic>
#include <iostream>
#include <memory>
#include <cassert>
#include <future>

/*
    Note that we absolutely do not need to have a shared pointer to the std::atomic - we should just have the node constructor take in a nonatomic value and just always use loads.
*/

template<typename T>
struct FlaggedRef
{
    FlaggedRef(T* ref = nullptr, bool flag = false) : 
        obj_ptr(reinterpret_cast<uintptr_t>(ref) | flag)
    {}
    bool get_flag()
    {
        return mask & obj_ptr;
    }
    void set_flag(bool flag)
    {
        if(flag)
        {
            obj_ptr |= mask;
        }
        else
        {
            obj_ptr &= ~mask;
        }
    }
    T operator*()
    {
        return *get_ptr();
    }
    T* operator->()
    {
        return get_ptr();
    }
    operator T*()
    {
        return get_ptr();
    }

    __attribute__((always_inline))
    T* get_ptr()
    {
        return reinterpret_cast<T*>(obj_ptr & ~mask);
    }
    void set_ptr(T* ptr)
    {
        obj_ptr = reinterpret_cast<uintptr_t>(ptr) | get_flag();
    }
    FlaggedRef<T> operator=(T* rhs)
    {
        obj_ptr = reinterpret_cast<uintptr_t>(rhs);
        return *this;
    }
private:
    static constexpr uintptr_t mask = 1;
    uintptr_t obj_ptr;
};

template<typename U>
bool compare_exchange_strong(
        std::atomic<FlaggedRef<U>>& src,
        U* old_value,
        U* new_value,
        bool old_flag,
        bool new_flag,
        std::memory_order order = std::memory_order_seq_cst
        )
{
    FlaggedRef<U> old_fr(old_value, old_flag);
    FlaggedRef<U> new_fr(new_value, new_flag);
    return src.compare_exchange_strong(old_fr, new_fr, order);

}


template<typename T>
struct Node
{
    int code;
    T value;
    std::shared_ptr<std::atomic<FlaggedRef<Node>>> next;
};

template<typename U>
struct Window
{
    FlaggedRef<Node<U>> pred;
    FlaggedRef<Node<U>> curr;
};

template<typename T>
std::pair<FlaggedRef<Node<T>>, FlaggedRef<Node<T>>>
find(FlaggedRef<Node<T>> head, int key)
{
    FlaggedRef<Node<T>> pred, curr, succ;

    retry: while(true)
    {
        pred = head;
        curr = pred->next->load();

        while(true)
        {
            succ = curr->next->load();

            // indicates that we need to remove curr
            while(succ.get_flag())
            {
                if(!compare_exchange_strong(
                    *pred->next,
                    curr.get_ptr(),
                    succ.get_ptr(),
                    false,
                    false))
                {
                    goto retry;
                }
                // need to do something with the curr ptr!
                // we are done with it
                // we also need to delete the atomic next
                // pointer to curr while we're at it...

                curr = succ;
                succ = succ->next->load();

            }

            if(curr->code >= key)
            {
                return std::make_pair(pred, curr);
            }

            pred = curr;
            curr = curr->next->load();
        }
    }
}


template<
    typename T>
struct Set
{
    Set(T init = T{}) : 
        tail{new Node<T>{
            std::numeric_limits<int>::max(), 
            init, 
            std::make_shared<std::atomic<FlaggedRef<Node<T>>>>()
            
        }},
        head{new Node<T>{
            0,
            init,
            std::make_shared<std::atomic<FlaggedRef<Node<T>>>>(tail)
        }},
        hash{}
    {}

    bool insert(T obj)
    {
        int code = hash(obj);

        while(true) 
        {
            auto [pred, curr] = find(head, code);

            if(curr->code == code)
            {
                return false;
            }

            Node<T>* new_node = 
                new Node<T>{code, obj, std::make_shared<std::atomic<FlaggedRef<Node<T>>>>(curr)};

            if(!compare_exchange_strong(
                    *pred->next,
                    curr.get_ptr(),
                    new_node,
                    false,
                    false
               ))
            {
                delete new_node;
                continue;
            }
            return true;
        }
    }


    bool remove(T obj)
    {
        int code = hash(obj);

        while(true)
        {
            auto [pred, curr] = find(head, code);

            if(curr->code != code)
            {
                return false;
            }
            auto succ = curr->next->load();

            if(!compare_exchange_strong(
                    *curr->next,
                    succ.get_ptr(),
                    succ.get_ptr(),
                    false,
                    true,
                    std::memory_order_acq_rel))
            {
                continue;
            }

            assert(curr->next->load().get_flag());

            compare_exchange_strong(
                *pred->next,
                curr.get_ptr(),
                succ.get_ptr(),
                false,
                false 
            );
            return true;

        }
    }

    bool contains(T obj)
    {
        int code = hash(obj);

        auto curr = head;

        while(curr->code < code)
        {
            curr = curr->next->load();
        }

        auto succ = curr->next->load();
        return curr->code == code && !succ.get_flag();
    }
private:
    FlaggedRef<Node<T>> tail;
    FlaggedRef<Node<T>> head;
    std::hash<T> hash;
};
