# ReDe Provided Commands

## Push ##

*syntex:* **PUSH** dehydrator_name element_id element ttl

*Available since: 0.1.0*

*Time Complexity: O(1)*

Push an `element` into the dehydrator for `ttl` seconds, marking it with `element_id`

Note: if the key does not exist this command will create a Dehydrator on it.

***Return Value***

"OK" on success, Error if key is not a dehydrator, or element with `element_id` already exists.

Example
```
redis> DEHYDRATOR.PUSH my_dehydrator 101 "Dehydrate this" 3
OK
redis> DEHYDRATOR.LOOK my_dehydrator 101
"Dehydrate this"
redis> DEHYDRATOR.POLL my_dehydrator
(empty list or set)
```
wait for 3 seconds
```
redis> DEHYDRATOR.POLL my_dehydrator
"Dehydrate this"
```


## PULL ##

*syntex:* **PULL** dehydrator_name element_id

*Available since: 0.1.0*

*Time Complexity: O(1)*

Pull the element corrisponding with `element_id` and remove it from the dehydrator before it expires.

***Return Value***

The element represented by `element_id` on success, Null if key is empty or not a dehydrator, or element with `element_id` does not exist.

Example
```
redis> DEHYDRATOR.PUSH my_dehydrator 101 "Dehydrate this" 3
OK
redis> DEHYDRATOR.PULL my_dehydrator 101
"Dehydrate this"
redis> DEHYDRATOR.PULL my_dehydrator 101
(nil)
```





* `DEHYDRATOR.POLL` - Pull and return all the expired elements.
* `DEHYDRATOR.LOOK` - Search the dehydrator for an element with the given id and if found return it's payload (without pulling).
* `DEHYDRATOR.UPDATE` - Set the element represented by a given id, the current element will be returned, and the new element will inherit the current expiration.
* `DEHYDRATOR.TTN` - Return the minimal time between now and the first expiration.
* `DEHYDRATOR.TEST`  - a set of unit tests of the above commands.

### time complexity

* `DEHYDRATOR.PUSH`  - O(1)
* `DEHYDRATOR.PULL`  - O(1)
* `DEHYDRATOR.POLL`  - O(max{N.M}) where N is the number of expired elements and M is the number of different TTLs elements were pushed with.
* `DEHYDRATOR.LOOK`  - O(1)
* `DEHYDRATOR.UPDATE` - O(1)
* `DEHYDRATOR.TTN` - O(M) where M is the number of different TTLs elements were pushed with.
* `DEHYDRATOR.TEST`  - Fixed time (~11 seconds) - this function uses `sleep` (dios mio, No! &#x271e;&#x271e;&#x271e;).

** usage example **
```
127.0.0.1:9979> DEHYDRATOR.PUSH some_dehy id1 world 15
OK
127.0.0.1:9979> DEHYDRATOR.PUSH some_dehy id2 hello 1
OK
127.0.0.1:9979> DEHYDRATOR.PUSH some_dehy id3 goodbye 2
OK
127.0.0.1:9979> DEHYDRATOR.PULL some_dehy id3
"goodbye"
127.0.0.1:9979> DEHYDRATOR.POLL some_dehy
1) "hello"
127.0.0.1:9979> DEHYDRATOR.POLL some_dehy
(empty list or set)
127.0.0.1:6379> DEHYDRATOR.LOOK some_dehy id2
(nil)
127.0.0.1:6379> DEHYDRATOR.LOOK some_dehy id1
"world"
127.0.0.1:6379> DEHYDRATOR.PULL some_dehy id2
(nil)
127.0.0.1:6379> DEHYDRATOR.TTN some_dehy
8
```

This `(empty list or set)` reply from `DEHYDRATOR.POLL` means that the there are no more items to pull right now, so we'll have to wait until enough time passes for our next element to expire. using DEHYDRATOR.TTN we can see this will be in 8 seconds (in this example we waited a bit between commands). Once 8 seconds will pass we can run:

```
127.0.0.1:9979> DEHYDRATOR.POLL some_dehy
1) "world"
127.0.0.1:9979> DEHYDRATOR.TEST
PASS
(15.00s)
127.0.0.1:9979> DEL some_dehy
OK
```

Enjoy!


## Future work

* Additional, more thorough tests

