#include <unordered_map>
#include <thread>
#include <iostream>
#include <cassert>

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


struct Widget
{
    Widget()
      : localInt(this, 5)
    {}
    ThreadLocal<Widget, int> localInt;
};

INIT_THREAD_LOCAL(Widget, int)

int main()
{
  Widget w{};

  std::thread th{[&w](){
    w.localInt.store(9);
    std::cout << w.localInt.load();
  }};

  th.join();
  std::cout << w.localInt.load() << "\n";
}


