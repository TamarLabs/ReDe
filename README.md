# ReDehy - The Redis Element Dehydration Module
An Simple Redis Module for data dehydration. This is a pretty straightforward implementation of the dehydration system depicted in the article "[Fast Data](https://goo.gl/DDFFPO)". The Goal of this module is to solve the *Contextual Completeness* and *Emergent Relevancy* problems by adding the ability to postpone incoming elements to a later time in which we will have a complete information for these elements. Effectively acting as a snooze button to any element.

From the article:
> Dehydrators are simplistic time machines. They transport data elements that arrived prematurely in terms of their context right to the future where they might be needed, without loading the system while waiting. This concept is achieved by attaching a time-indexed data store to a clock, storing elements as they arrive to the dehydrator and re-introducing them as inputs to the system once a predetermined time period has passed.

Using this system it is also possible to craft a self cleaning "claims check", to minimize load on transportation and manipulation nodes of a pipeline architecture.

You can read further on the algorithm behind this module [here](Algorithm.md).

The module works by adding a new type to Redis -`DehydratorType`. It will be ceated automatically when you call a push command on it, and it can be deleted using the `DEL` command like any other key.

![a gif that shows basic usage](redehy-basics.gif)

## What it includes:

### 1. Dehydration module source code

[module.c](module.c) - Build it, read it, love it, extend it (PRs are welcome)!

### 2. usage example files and load tests

In this repository there are two python files that exemplify the usage of the module:
* [helloworld.py](helloworld.py) - very simple usage example of all the functions exposed by the module
* [test.py](test.py) - run internal as well as external functional tests, load test and print it all to stdout.

### 3. klib [khash](khash.h)

A set of macros to create the hash maps used to implement the dehydrator type.

### 4. LibRMUtil

From [Redis Modules SDK](https://github.com/RedisLabs/RedisModulesSDK) README:

A small library of utility functions and macros for module developers, including:

* Easier argument parsing for your commands.
* Testing utilities that allow you to wrap your module's tests as a redis command.
* `RedisModuleString` utility functions (formatting, comparison, etc)
* The entire `sds` string library, lifted from Redis itself.
* A generic scalable Vector library. Not redis specific but we found it useful.
* A few other helpful macros and functions.
* `alloc.h`, an include file that allows modules implementing data types to implicitly replace the `malloc()` function family with the Redis special allocation wrappers.

It can be found under the `rmutil` folder, and compiles into a static library you link your module against.    

## Usage

The dehydrator is an effective 'snooze button' for events, you push an event into it along with an id (for future referance) and in how many seconds you want it back, and poll whenever you want the elements back. only expired elements would pop out.

**It includes 4 main commands:**

* `DEHYDRATOR.PUSH` - push an element, it will need an id, the element itself and dehydration time in seconds.
* `DEHYDRATOR.PULL` - pull the element with the appropriate id before it expires.
* `DEHYDRATOR.POLL` - efficiently return all the expired elements.
* `DEHYDRATOR.LOOK` - search the dehydrator for an element with the given id and if found return it's payload (without pulling).

**The module also includes 2 assistive commands:**
* `DEHYDRATOR.TEST`  - a set of unit tests of the above commands.

### time complexity

* `DEHYDRATOR.PUSH`  - O(1)
* `DEHYDRATOR.PULL`  - O(N) where N is the number of dehydrating elements with the same TTL.
* `DEHYDRATOR.POLL`  - O(N) where N is the number of expired elements, please notice we regard # of different TTLs << # of dehydrated elements in the system.
* `DEHYDRATOR.LOOK`  - O(1)
* `DEHYDRATOR.TEST`  - Fixed time (~11 seconds) - this function uses `sleep` (dios mio, No! &#x271e;&#x271e;&#x271e;).

### Quick Start Guide

Here's what you need to do to build this module:

0. Build Redis in a build supporting modules.
1. Build the module: `make`
3. Run Redis loading the module: `/path/to/redis-server --loadmodule ./example/module.so`

Now run `redis-cli` and try the commands:

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
ERROR: No Such Element
127.0.0.1:6379> DEHYDRATOR.LOOK some_dehy id1
"world"
127.0.0.1:6379> DEHYDRATOR.PULL some_dehy id2
ERROR: No Such Element
```

This `(empty list or set)` reply from `DEHYDRATOR.POLL` means that the there are no more items to pull right now, so we'll have to wait until enough time passes for our next element to be ready (15 seconds in this case). Then we can run:

```
127.0.0.1:9979> DEHYDRATOR.POLL some_dehy
1) "world"
127.0.0.1:9979> DEHYDRATOR.TEST
PASS
(11.00s)
127.0.0.1:9979> DEL some_dehy
OK
```

Enjoy!


## Future work

* add UPDATE command
* "time to next element" command
* add pub/sub mechanism to POLL
* a whole lot more

## About This Module

This module is based off a python version of the same concepts developed in Tamar Labs by Adam Lev-Libfeld and Alexander Margolin and designed by Adam Lev-Libfeld in mid 2015.

The Redis module was created by Adam Lev-Libfeld during the RedisModulesHackathon in late 2016, and is maintained by him solely.
