#include <unordered_map>
#include <thread>
#include <iostream>
#include <cassert>
#include <type_traits>

#define INIT_THREAD_LOCAL(class_type, object_type) \
    template<> \
    thread_local std::unordered_map<std::add_pointer_t<class_type>, object_type> ThreadLocal<class_type, object_type>::_map{};

template<typename Container, typename T>
struct ThreadLocal
{
    ThreadLocal(Container* self, T init)
      : self(self)
    {
        _map.insert({self, init});
    }
    ThreadLocal(Container* self)
      : self(self)
    {}

    void store(T object)
    {
        auto it = _map.find(self);

        if(it == _map.end())
        {
            _map.insert({self, object});
        }
        else
        {
            it->second = object;
        }
    }

    T &load()
    {
        auto it = _map.find(self);
        assert(it != _map.end()
            && "variable must be initialized");

        return it->second;
    }

    ~ThreadLocal()
    {
        _map.erase(self);
    }
private:
    Container* self;
    static thread_local std::unordered_map<Container*, T> _map;
};
