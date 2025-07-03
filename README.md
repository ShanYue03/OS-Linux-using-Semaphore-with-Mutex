# OS-Linux-using-Semaphore-with-Mutex
A fully-developed traffic control in operating systems based on Semaphore and Mutual exclusion techniques, which show strong adaptation in solving real-world scenario problems
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

## An overview of the source code:

### Main Function Logic:
In main(), the simulation is set up and launched. First, signal handling and random seeding are initialized. The semaphore controlling construction‐lane capacity is created, and the log buffer is primed with an initial “Simulation started.” entry. Three cars per direction are preloaded into their respective queues under a mutual‐exclusion lock. Finally, five threads are spawned to handle light switching, UI updates, car generation, dispatching, and timekeeping; the program waits (pthread_join) for each to finish before cleaning up and exiting.

### Mutex Logic:
Two mutexes protect shared state: one guards all accesses to queues, lane data, and the traffic‐light direction, ensuring that enqueueing cars, moving them into/out of the construction lane, and toggling the light happen atomically; the other protects the log buffer so that concurrent log_event() calls don’t interleave text. Wherever shared arrays or counters are read or written, the appropriate mutex is locked before the operation and unlocked immediately afterward, preventing race conditions.

### Semaphore Logic:
A counting semaphore initialized to the lane capacity (2) enforces the maximum number of cars allowed in the construction zone simultaneously. Each car thread calls sem_wait() before entering the lane, blocking if the zone is full and calls sem_post() after exiting to free a slot for others. This ensures that, regardless of how many cars or ambulances are queued, no more than two occupy the construction lane at once.

