# Dehydration Algorithms
> Dehydrators are simplistic time machines. They transport data elements that arrived prematurely in terms of their context right to the future where they might be needed, without loading the system while waiting. This concept is achieved by attaching a time-indexed data store to a clock, storing elements as they arrive to the dehydrator and re-introducing them as inputs to the system once a predetermined time period has passed.                                                      *From:* [Fast Data](https://goo.gl/DDFFPO)

In general, the goal of a dehydration mechanism is to store data for a predetermined time, and releasing it upon request without loading the using system.
That is, beyond being speedy and resilient, we want all operations on this time centric data-store to be have a minimal signature, using just IDs when possible (minimizing cross-process communication).

Storing Data in a DB based on a key of id and indexed by time is a common technique in construction of all time dependent systems, but using Redis as a Key-Value store enabled more flexibility when constructing a data-store for the specific goal of being a dehydrator.

## Naive Algorithm 1

These goals can be achieved relatively easily using naive implementation of a unified list where items are being stored as an `(id, element, insert time + TTL)`. tuple and are added to the list (Push) in O(1). The obvious down side is that pulling a specific element from the list (Pull) is done at O(n) and polling for expired items is also done at O(n), where n is the number of all stored elements in the list.
so:
* Push - O(1)
* Pull - O(n)
* Poll - O(n)


## Naive Algorithm 2

An improvement can be done to this algorithm using a queue instead of a the list where items are being stored as the same tuple, but are sorted based on expiration time, not insertion time. This would skew runtimes to be:
* Push - O(n) (or at best O(log n)), since now we need to scan the queue in order to find where to insert the new element.
* Pull - still done at O(n)
* Poll - is now improved to O(m) where m is the number of **expired** items


## Queue-Map Algorithm

**this is the algorithm that is used in this module**
This Algorithm takes the best from both algorithms, and combines it in a new manner - the idea behind assumes you will have a set of different TTL which is significantly smaller (<<) in size then the amount of items that are stored. Using this assumption we could now store items in a queue based on their TTL. Each queue would be self-sorted by expiration due to the fact that each two consecutive events with the same TTL would have expiration times that correspond with the order in which they have arrived. Polling is done by iterating over the queues and from each queue pop all the elements which expired, once you see an element that has not expired yet, move to next queue.
Using these rules, and by holding a map of sorted TTL queues we can now:

* Push in O(1) since pulling the TTL from the map takes O(1) and inserting at the head of the queue is also O(1).
* Pull in O(n), but now n is the number of just the elements with the same TTL.
* Poll in O(n) - where n is minimized to just the number of expired elements, notice we regard the number of different TTLs to be a constant and << # of dehydrated elements in the system.
