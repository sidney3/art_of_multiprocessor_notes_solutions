#pragma once
#include <atomic>
#include <mutex>
#include <cassert>
#include <future>

/*
A lazy synchronization implementation of a set

Implemented assuming all items have a unique hash code. Searching is just
a bit more annoying if this is not the case.
*/
using mutex_lock = std::unique_lock<std::mutex>;

template<typename T>
struct node
{
    node(T item, int code, std::shared_ptr<node> next = nullptr)
    : item(item), code(code), marked(false), lock{}, next(std::move(next))
    {}
    T item;
    int code;
    std::atomic<bool> marked;
    std::mutex lock;
    std::shared_ptr<node> next;
};

template<typename T>
struct set
{
    set(T filler = T{}) :
      tail(std::make_shared<node<T>>(filler, 1e8)),
      head(std::make_shared<node<T>>(filler, 0, tail)),
      hasher{}
    {}
    bool insert(T item);
    bool remove(T item);
    bool contains(T item) const;
    bool validate(const std::shared_ptr<node<T>>& pred, const std::shared_ptr<node<T>>& curr)const
    {
        return !pred->marked && !curr->marked && pred->next == curr;
    }
  private:
    std::shared_ptr<node<T>> tail;
    std::shared_ptr<node<T>> head;
    std::hash<T> hasher;
};

template<typename T>
bool set<T>::insert(T item)
{
  int code = hasher(item);
    while(true)
    {
      auto pred = head;
      auto curr = head->next;

      while(curr->code < code)
      {
          pred = curr;
          curr = curr->next;
      }

      
      std::unique_lock pred_lock{pred->lock};
      std::unique_lock curr_lock{curr->lock};

      if(!pred->marked && curr->code == code)
      {
          return false;
      }

      if(validate(pred, curr))
      {
          pred->next = std::make_shared<node<T>>(item, code, curr);
          return true;
      }

    }
    __builtin_unreachable();
}

template<typename T>
bool set<T>::remove(T item)
{
    int code = hasher(item);
    while(true)
    {
        auto pred = head;
        auto curr = pred->next;

        while(curr->code < code)
        {
            pred = curr;
            curr = curr->next;
        }

        std::unique_lock pred_lock{pred->lock};
        std::unique_lock curr_lock{curr->lock};

        // item is not in the set
        if(curr->code != code)
        {
            return false;
        }

        if(validate(pred, curr))
        {
            curr->marked = true;
            pred->next = curr->next;
            return true;
        }
    }
    __builtin_unreachable();
}
template<typename T>
bool set<T>::contains(T item) const
{
    int target_code = hasher(item);
    auto curr = head;
    while(curr != tail)
    {
        if(!curr->marked && curr->code == target_code)
        {
            return true;
        }
        curr = curr->next;
    }
    return false;
}
