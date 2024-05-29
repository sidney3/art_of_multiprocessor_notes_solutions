#pragma once
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>
#include <cassert>
#include <atomic>
#include <optional>

/*
    The more efficient implementation of a stamped reference: for a type T we store only 
    a T* value and use the bits that we know will be 0 (for alignment reasons) to store
    the stamp.

    Note that in order to allow incomplete types T, we make direct reference to T outside of methods
*/
template<typename T>
struct alignas(sizeof(uintptr_t)) StampedRefStealing
{
    constexpr StampedRefStealing(T* ptr, const size_t init_stamp = 0)
        : _ptr(reinterpret_cast<uintptr_t>(ptr))
    {
        setStamp(init_stamp);
    }
    constexpr void setStamp(size_t new_stamp)
    {
        testStamp(new_stamp);
        _ptr = (_ptr & ~mask()) | new_stamp;
    }
    constexpr size_t getStamp()
    {
        return _ptr & mask();
    }
    void incStamp()
    {
        size_t oldStamp = getStamp();
        setStamp((oldStamp + 1) % max_stamp());
    }

    T* operator->()
    {
        return get_ptr();
    }

    T& operator*()
    {
        return *get_ptr();
    }


    bool operator==(const T* rhs)
    {
        return get_ptr() == rhs;
    }
    T* get_ptr()
    {
        return reinterpret_cast<T*>(_ptr & ~mask());
    }
private:
    void testStamp(size_t new_stamp)
    {
        if(__builtin_constant_p(new_stamp))
        {
            if(new_stamp > max_stamp())
            {
              [[gnu::error("out-of-bounds sequence access detected")]] void static_bounds_check_failed();
              static_bounds_check_failed();  
            }
        }
        else
        {
            assert(new_stamp <= max_stamp());
        }
    }
    static constexpr uintptr_t mask()
    {
        return std::alignment_of_v<T> - 1;
    }
    static constexpr size_t max_stamp()
    {
        return mask();
    }
    uintptr_t _ptr;
};

template<typename T>
struct alignas(2*sizeof(uintptr_t)) StampedRefNormal
{
    constexpr StampedRefNormal(T* ptr, const size_t init_stamp = 0)
        : _ptr(reinterpret_cast<uintptr_t>(ptr)),
        _stamp(init_stamp)
    {
        assert(!(reinterpret_cast<uintptr_t>(ptr) & (alignof(T) - 1)) &&
                "pointer not correctly aligned");
        assert(reinterpret_cast<uintptr_t>(ptr) % alignof(T) == 0 && "Pointer is not properly aligned");
    }

    void setStamp(size_t new_stamp)
    {
        _stamp = static_cast<uintptr_t>(new_stamp);
    }
    size_t getStamp()
    {
        return _stamp;
    }
    void incStamp()
    {
        size_t oldStamp = getStamp();
        setStamp((oldStamp + 1) % std::numeric_limits<uintptr_t>::max());
    }

    T* operator->()
    {
        return get_ptr();
    }
    T& operator*()
    {
        return *get_ptr();
    }
    T* get_ptr()
    {
        uintptr_t mask = alignof(T) - 1;
        if(_ptr & mask)
        {
            std::cout << "misaligned pointer to type" << typeid(T).name() << " with alignment: " << alignof(T) << " " << reinterpret_cast<uintptr_t>(_ptr) << "\n";
        }
        assert(!(_ptr & mask));
        return reinterpret_cast<T*>(_ptr);
    }

    bool operator==(const T* rhs)
    {
        return rhs == get_ptr();
    }

private:
    uintptr_t _ptr;
    uintptr_t _stamp;
};


/*
Very lightweight linked list. Optimized for small number of allocations and one off accesses.
*/
template<typename T>
struct FreeList
{
    FreeList()
        : head(nullptr)
    {}
    std::optional<T> pop()
    {
        if(empty())
        {
            return std::nullopt;
        }
        auto res = std::make_optional<T>(head->item);
        head = std::move(head->prev);
        return res;
    }
    template<typename U>
        requires std::constructible_from<T,U>
    void push(U&& item)
    {
        head = std::make_unique<Node>(std::forward<U>(item), std::move(head));
    }
    bool empty() const
    {
        return head == nullptr;
    }
private:
    struct Node
    {
        T item;
        std::unique_ptr<Node> prev;
    };
    std::unique_ptr<Node> head;

};

template<
    typename T,
    typename StampedRef = StampedRefNormal<T>,
    typename BaseAllocator = std::allocator<T>
    >
struct StampedAllocator
{
    using AllocTraits = std::allocator_traits<BaseAllocator>;

    template<typename ... Args>
    [[nodiscard]]
    StampedRef construct(Args&&... args)
    {
        if(freeList.empty())
        {
            T* baseRef = std::allocator_traits<BaseAllocator>::allocate(_alloc, 1);
            new (&baseRef[0]) T {std::forward<Args>(args)...};
            return StampedRef{baseRef};
        }
        else
        {
            StampedRef freeNode = *freeList.pop();
            new (freeNode.get_ptr()) T {std::forward<Args>(args)...};
            freeNode.incStamp();
            return freeNode;
        }
    }

    void free(StampedRef ref)
    {
        assert(ref != nullptr
                && "freeing nullptr");
        ref->~T();
        freeList.push(std::move(ref));
    }

    ~StampedAllocator()
    {
        while(!freeList.empty())
        {
            StampedRef top = *freeList.pop();
            std::allocator_traits<BaseAllocator>::deallocate(_alloc, top.get_ptr(), 1);

        }
    }

private:
#if defined(__has_cpp_attribute) && __has_cpp_attribute(no_unique_address)
    [[no_unique_address]]
    BaseAllocator _alloc;
#else
    BaseAllocator _alloc;
#endif
    FreeList<StampedRef> freeList;
};

