# ReDe Provided Commands

### Commands described in this document

1. [`REDE.PUSH`](#push)
2. [`REDE.GIDPUSH`](#gidpush)
3. [`REDE.PULL`](#pull)
4. [`REDE.POLL`](#poll)
5. [`REDE.LOOK`](#look)
6. [`REDE.TTN`](#ttn)
7. [`REDE.UPDATE`](#update)

### Performance of main commands in events/second by version
| Command       | 0.1.0  |  0.2.0 < |  0.3.0 <  |
| ------------- |:------:|:-------:|:-------:|
| **PUSH**      | 16,000 |  23,000 |  22,000 |
| **PULL**      | 19,500 |  31,000 |  31,500 |
| **POLL**      |  1,700 | 265,000 | 305,000 |

for requests/second `redis-benchmark` results can be found [here](redis-benchmark_results.md)

## PUSH ##

*syntex:* **PUSH** dehydrator_name ttl element element_id

*Available since: 0.1.0*

*Time Complexity: O(1)*

Push an `element` into the dehydrator for `ttl` milliseconds, marking it with `element_id`

Note: if the key does not exist this command will create a Dehydrator on it.

***Return Value***

"OK" on success, Error if key is not a dehydrator or if an element with `element_id` already exists.

Example
```
redis> REDE.PUSH my_dehydrator 3000 "Dehydrate this" 101
OK
redis> REDE.LOOK my_dehydrator 101
"Dehydrate this"
redis> REDE.POLL my_dehydrator
(empty list or set)
```
wait for 3 seconds
```
redis> REDE.POLL my_dehydrator
"Dehydrate this"
```


## GIDPUSH ##

*syntex:* **GIDPUSH** dehydrator_name ttl element

*Available since: 0.4.0*

*Time Complexity: O(1)*

Push an `element` into the dehydrator for `ttl` milliseconds, marking it with an *auto-generated* `element_id`
This command is slower then `PUSH` as the GUID generating process takes time.

Note: if the key does not exist this command will create a Dehydrator on it.

***Return Value***

The generated GUID on success, Error if key is not a dehydrator.

Example
```
redis> REDE.GIDPUSH my_dehydrator 3000 "Dehydrate this"
SFTL5409QWRAR9EPU2W4NG4S6D8T2TZ
redis> REDE.LOOK my_dehydrator SFTL5409QWRAR9EPU2W4NG4S6D8T2TZ
"Dehydrate this"
redis> REDE.POLL my_dehydrator
(empty list or set)
```
wait for 3 seconds
```
redis> REDE.POLL my_dehydrator
"Dehydrate this"
```


## PULL ##

*syntex:* **PULL** dehydrator_name element_id

*Available since: 0.1.0*

*Time Complexity: O(1)*

Pull the element corresponding with `element_id` and remove it from the dehydrator before it expires.

***Return Value***

The element represented by `element_id` on success, Null if key is empty or not a dehydrator, or element with `element_id` does not exist.

Example
```
redis> REDE.PUSH my_dehydrator 3000 "Dehydrate this" 101
OK
redis> REDE.PULL my_dehydrator 101
"Dehydrate this"
redis> REDE.PULL my_dehydrator 101
(nil)
```

## POLL ##

*syntex:* **POLL** dehydrator_name

*Available since: 0.1.0*

*Time Complexity: O(max{N.M}) where N is the number of expired elements and M is the number of different TTLs elements were pushed with. *

Pull and return all the expired elements in `dehydrator_name`.

***Return Value***

List of all expired elements on success, or an empty list if no elements are expired, the key is empty or the key contains something other the a dehydrator.

Example
```
redis> REDE.PUSH my_dehydrator 3000 "Dehydrate this" 101
OK
redis> REDE.PUSH my_dehydrator 1000 "Dehydrate that" 102
OK
redis> REDE.POLL my_dehydrator
(empty list or set)
```
wait for 1 second
```
redis> REDE.POLL my_dehydrator
("Dehydrate that")
```
wait additional 2 seconds
```
redis> REDE.POLL my_dehydrator
("Dehydrate this")
```

## XPOLL ##

*syntex:* **XPOLL** dehydrator_name

*Available since: 0.5.0*

*Time Complexity: O(max{N.M}) where N is the number of expired elements and M is the number of different TTLs elements were pushed with. *

Return the IDs of all the expired elements in `dehydrator_name`, ***without pulling***.

***Return Value***

List of IDs for all expired elements on success, or an empty list if no elements are expired, the key is empty or the key contains something other the a dehydrator.

Example
```
redis> REDE.PUSH my_dehydrator 3000 "Dehydrate this" 101
OK
redis> REDE.PUSH my_dehydrator 1000 "Dehydrate that" 102
OK
redis> REDE.XPOLL my_dehydrator
(empty list or set)
```
wait for 1 second
```
redis> REDE.XPOLL my_dehydrator
1) "102"
```
wait additional 2 seconds
```
redis> REDE.XPOLL my_dehydrator
1) "101"
2) "102"
```


## XACK ##

*syntex:* **XPOLL** dehydrator_name

*Available since: 0.5.0*

*Time Complexity: O(max{N.M}) where N is the number of expired elements and M is the number of different TTLs elements were pushed with. *

Pull and return all the expired elements of `dehydrator_name` from within the given set of IDs.

***Return Value***

List of all expired elements on success, populated with `(nil)`s wherever an error has occured. If no elements are expired, the key is empty or the key contains something other the a dehydrator, an empty list will be returned.

Example
```
redis> REDE.PUSH my_dehydrator 3000 "Dehydrate this" 101
OK
redis> REDE.PUSH my_dehydrator 1000 "Dehydrate that" 102
OK
```
wait for 1 second
```
redis> REDE.POLL my_dehydrator
1) "Dehydrate that"
```
wait additional 2 seconds
```
redis> REDE.XPOLL my_dehydrator
(101)
redis> REDE.XACK my_dehydrator 101 102 103
1) "Dehydrate this"
2) (nil)
3) (nil)
```


## LOOK ##

*syntex:* **LOOK** dehydrator_name element_id

*Available since: 0.1.0*

*Time Complexity: O(1)*

Show the element corresponding with `element_id` and without removing it from the dehydrator.

***Return Value***

The element represented by `element_id` on success, Null if key is empty or not a dehydrator, or element with `element_id` does not exist.

Example
```
redis> REDE.PUSH my_dehydrator 3000 "Dehydrate this" 101
OK
redis> REDE.PUSH my_dehydrator 3000 "Dehydrate that" 102
OK
redis> REDE.LOOK my_dehydrator 101
"Dehydrate this"
redis> REDE.LOOK my_dehydrator 102
"Dehydrate that"
redis> REDE.LOOK my_dehydrator 102
(nil)
```



## TTN ##

*syntex:* **TTN** dehydrator_name

*Available since: 0.2.1*

*Time Complexity: O(M) where M is the number of different TTLs elements were pushed with.

Show the time left (in milliseconds) until the next element will expire.

***Return Value***

int representing the number of milliseconds until next element will expire. Null if `dehydrator_name` does not contain a dehydrator.

Example
```
redis> REDE.PUSH my_dehydrator 3000 "Dehydrate this" 101
OK
redis> REDE.PUSH my_dehydrator 1000 "Dehydrate that" 102
OK
redis> REDE.TTN my_dehydrator
1000
```
wait for 1 second
```
redis> REDE.TTN my_dehydrator
0
redis> REDE.POLL my_dehydrator
("Dehydrate that")
redis> REDE.TTN my_dehydrator
2000
```


## UPDATE ##

*syntex:* **UPDATE** dehydrator_name element_id new_element

*Available since: 0.2.1*

*Time Complexity: O(1)*

Change the element corresponding with `element_id` with `new_element` and return the original.

***Return Value***

The element that *was* represented by `element_id` on success, Error if key is empty or not a dehydrator, or if element with `element_id` does not exist.

Note: the expiration time of `new_element` will not be the same as the original element.

Example
```
redis> REDE.PUSH my_dehydrator 3000 "Dehydrate this" 101
OK
redis> REDE.UPDATE my_dehydrator 101 "Dehydrate that"
"Dehydrate this"
redis> REDE.LOOK my_dehydrator 101
"Dehydrate that"
```
