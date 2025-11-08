# Cache-Simulator
Basic functionality:
CACHE is configurable in terms of supporting any cache size, associativity, and block size, specified at the beginning of simulation: 
o SIZE: Total bytes of data storage. 
o ASSOC: The associativity of the cache. (ASSOC = 1 is a direct-mapped cache.  ASSOC = # blocks in the cache = SIZE/BLOCKSIZE is a fully-associative cache.) 
o BLOCKSIZE: The number of bytes in a block. 
There are a few constraints on the above parameters: 1) BLOCKSIZE is a power of two and 2) the number of sets is a power of two. Note that ASSOC (and, therefore, SIZE) need not be a 
power of two.The number of sets is determined by:  SIZE/ (ASSOC * BLOCKSIZE)

Replacement policy:
CACHE uses the LRU (least-recently-used) replacement policy.

Write policy:
CACHE uses the WBWA (write-back + write-allocate) write policy. 
o Write-allocate: A write that misses in CACHE will cause a block to be allocated in CACHE. Therefore, both write misses and read misses cause blocks to be allocated in CACHE. 
o Write-back: A write updates the corresponding block in CACHE, making the block dirty. It does not update the next level in the memory hierarchy (next level of cache or memory). If a dirty block is evicted from 
CACHE, a writeback (i.e., a write of the entire block) will be sent to the next level in the memory hierarchy. 

Block allocation:
CACHE receives a read or write request from whatever is above it in the memory hierarchy (either the CPU or another cache). The only situation where CACHE interacts with the next level below it (either another 
CACHE or main memory) is when the read or write request misses in CACHE. When the read or write request misses in CACHE, it allocates the requested block so that the read or write can be performed. 

When allocating a block, CACHE issues a write request (only if there is a victim block and it is dirty) followed by a read request, both to the next level of the memory hierarchy. 
Each of these two requests could themselves miss in the next level of the memory hierarchy (if the next level is another CACHE), causing a cascade of requests in subsequent levels. 

State Updating:
After servicing a read or write request, whether the corresponding block was in the cache already (hit) or had just been allocated (miss), the other state is updated. This state includes LRU 
counters affiliated with the set as well as the valid and dirty bits affiliated with the requested block. 


