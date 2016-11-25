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


## PUSH ##

*syntex:* **PUSH** dehydrator_name ttl element element_id

*Available since: 0.1.0*

*Time Complexity: O(1)*

Push an `element` into the dehydrator for `ttl` seconds, marking it with `element_id`

Note: if the key does not exist this command will create a Dehydrator on it.

***Return Value***

"OK" on success, Error if key is not a dehydrator or if an element with `element_id` already exists.

Example
```
redis> REDE.PUSH my_dehydrator 3 "Dehydrate this" 101
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

*Available since: 0.1.0*

*Time Complexity: O(1)*

Push an `element` into the dehydrator for `ttl` seconds, marking it with an *auto-generated* `element_id`
This command is slower then `PUSH` as the GUID generating process takes time.

Note: if the key does not exist this command will create a Dehydrator on it.

***Return Value***

The generated GUID on success, Error if key is not a dehydrator.

Example
```
redis> REDE.GIDPUSH my_dehydrator 3 "Dehydrate this"
SDDF126547398RTO2WC3SDFYEJU5HB
redis> REDE.LOOK my_dehydrator SDDF126547398RTO2WC3SDFYEJU5HB
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
redis> REDE.PUSH my_dehydrator 101 "Dehydrate this" 3
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
redis> REDE.PUSH my_dehydrator 101 "Dehydrate this" 3
OK
redis> REDE.PUSH my_dehydrator 102 "Dehydrate that" 1
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


## LOOK ##

*syntex:* **LOOK** dehydrator_name element_id

*Available since: 0.1.0*

*Time Complexity: O(1)*

Show the element corresponding with `element_id` and without removing it from the dehydrator.

***Return Value***

The element represented by `element_id` on success, Null if key is empty or not a dehydrator, or element with `element_id` does not exist.

Example
```
redis> REDE.PUSH my_dehydrator 101 "Dehydrate this" 3
OK
redis> REDE.PUSH my_dehydrator 102 "Dehydrate that" 3
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

Show the time left (in seconds) until the next element will expire.

***Return Value***

int representing the number of seconds until next element will expire. Null if `dehydrator_name` does not contain a dehydrator.

Example
```
redis> REDE.PUSH my_dehydrator 101 "Dehydrate this" 3
OK
redis> REDE.PUSH my_dehydrator 102 "Dehydrate that" 1
OK
redis> REDE.TTN my_dehydrator
1
```
wait for 1 second
```
redis> REDE.TTN my_dehydrator
0
redis> REDE.POLL my_dehydrator
("Dehydrate that")
redis> REDE.TTN my_dehydrator
2
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
redis> REDE.PUSH my_dehydrator 101 "Dehydrate this" 3
OK
redis> REDE.UPDATE my_dehydrator 101 "Dehydrate that"
"Dehydrate this"
redis> REDE.LOOK my_dehydrator 101
"Dehydrate that"
```
