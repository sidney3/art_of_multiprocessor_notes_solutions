# Multithreaded Allocator

Requirements: returns stamped pointers (not that great of a requirement), and only frees at the end

Thread local allocation is greatly preferred

THe principle challenge is that frequently thread behavior is as follows:

- Many producer threads
- Many reader threads

And so if we just have thread local free lists of nodes, very quickly the memory will become inefficient.

## Prefer thread local execution


## Some way to mesh the memory together at the end of execution

Also note that at the end of each execution (when destructor of the allocator gets called), we need to confer with all the threads.
Basically there should be a global registry of the data structures that each thread uses to store its local memory.
Or better yet, we can have a wrapper for whatever this data structure is that turns it into a pool.

## Some way to match threads with too many allocations with those with very few allocations

We somehow want the global allocator object to act as a mediator between threads that are __consumers__ and __producers__. This should be used sparingly, because the ultimate goal is to have very few threads.

For this, we can actually use our mediator! Should we always be trying to make a connection? Not necessarily -> ideally we can perform a handoff of many excess allocations.

This suggests a slightly more complicated mediator - you make a request and perhaps specify a maximum amount of memory that you want to receive? Don't really want threads to get a firehose of memory. Another case is single producer many consumers -> perhaps we want to be a bit more intelligent in this case about how we send off our memory.

No need to have a synchronous exchange -> can just hand off to the central allocator. Note that if we are building an async queue, we can pop off more than one element - just need to recoil the pointer that we want a bit more.

## Dependency injection for both the thread handling and the main allocator.

What is the interface for each of these?

The main allocator is just a backup allocator for the children. The child allocators should never make calls to memory. In our allocator model, the "worse case" allocator that would normally be just plain malloc is, in this case, the Multithreaded allocator.
