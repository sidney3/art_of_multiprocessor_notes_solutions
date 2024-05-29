#pragma once
#include <mutex>
#include <vector>
#include <iostream>
#include <cassert>
#include "../StampedAllocator/StampedAllocator.h"

template<typename T>
using StampedRef = StampedRefNormal<T>;

template<typename T>
class ThreadSafeAllocator {
public:
    using value_type = T;

    ThreadSafeAllocator() = default; 

    StampedRef<T> allocate() {
        std::lock_guard<std::mutex> lock{mtx};
        if (free_list.empty()) {
            T* ptr = static_cast<T*>(::operator new(sizeof(T)));
            return StampedRef<T>{ptr};
        } else {
            StampedRef<T> ptr = free_list.back();
            free_list.pop_back();
            return ptr;
        }
    }

    void deallocate(StampedRef<T> p) {
        std::lock_guard<std::mutex> lock{mtx};
        /* auto it = std::find(free_list.begin(), free_list.end(), p); */
        /* assert(it == free_list.end() && "double free"); */
        p.incStamp();
        free_list.push_back(p);
    }

    ~ThreadSafeAllocator()
    {
        std::cout << "dealloc begin\n";
        while(!free_list.empty())
        {
            StampedRef<T> back = free_list.back();
            free_list.pop_back();
            ::operator delete(back.get_ptr(), sizeof(T));
        }
    }

private:
    std::vector<StampedRef<T>> free_list;
    std::mutex mtx;
};
