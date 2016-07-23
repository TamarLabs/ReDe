#include "redismodule.h"
#include "rmutil/util.h"
#include "rmutil/strings.h"
#include "rmutil/test_util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>

//Name of a hash-map mapping element IDs to queue timeouts (integers)
#define REDIS_QUEUE_MAP "element_queues"

//Name of a hash-map mapping element IDs to elements (JSON strings)
#define REDIS_ELEMENT_MAP "element_objects"

// Name of a hash-map mapping element IDs to elements (JSON strings)
#define REDIS_EXPIRATION_MAP "element_expiration"

// Format of the queue name (for storage)
#define REDIS_QUEUE_NAME_FORMAT "timeout_queue#%d"
#define REDIS_QUEUE_NAME_FORMAT_PATTERN "timeout_queue#*"

#define REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT "%s:element_dehydrating"


bool test_is_element_dehydrating_now(self, element_id)
{
    return self._redis.exists(REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT % element_id)
}



void set_element_dehydrating(self, element_id)
{
    self._redis.set(REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT % element_id, True)
}

void unset_element_dehydrating(self, element_id)
{
    self._redis.delete(REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT % element_id)
}

void clear(self)
{
    print "Flushing ALL Redis"
    # CAREFUL! (for testing purposes only)
    self._redis.flushall()
    print "Done."
}

bool test_set_is_element_dehydrating_now(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    if (test_is_element_dehydrating_now(element_id))
        return true
    set_element_dehydrating(element_id)
    return false

}

/*
* dehydrator.push <element_id> <element> <timeout>
* dehydrate <element> for <timeout> seconds
*/
int PushCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    // we need EXACTLY 4 arguments  TODO: make sure what is in argv[0]
    if (argc != 4)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);

    element_id = argv[1];
    element = argv[2];
    timeout = argv[3];

    if (test_set_is_element_dehydrating_now(element_id))
    {
        //return RedisModule_ReplyWithError(ctx, "ERROR: Element already dehydrating.");
        RedisModule_ReplyWithError(ctx, "ERROR: Element already dehydrating.");
        return REDISMODULE_ERR;
    }


    // make sure we have the queue listed
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "HSET", "css", REDIS_QUEUE_MAP, element_id, timeout);
    RMUTIL_ASSERT_NOERROR(srep);

    // add the element to the dehydrating elements map
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "HSET", "css", REDIS_ELEMENT_MAP, element_id, element);
    RMUTIL_ASSERT_NOERROR(srep);


    time_t result = time(NULL);
    // if(result != -1) //TODO: this seatbelt may be needed
    expiration = (uintmax_t)result + timeout;

    // add the element to the element expiration hash
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "HSETNX", "csl", REDIS_EXPIRATION_MAP, element_id, expiration);
    RMUTIL_ASSERT_NOERROR(srep);


    // add the elemet id to the actual dehydration queue
    char dehydration_queue_name[30];
    sprintf(dehydration_queue_name, REDIS_QUEUE_NAME_FORMAT, timeout);
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "RPUSH", "cs", dehydration_queue_name, element_id);
    RMUTIL_ASSERT_NOERROR(srep);
    free(dehydration_queue_name);//TODO: is this needed?

    return REDISMODULE_OK;
}


RedisModuleString* _pull(RedisModuleCtx *ctx, RedisModuleString * element_id, RedisModuleString * timeout_queue)
{

    RedisModuleString *element = NULL
    print 'pulling out from dehydrator', element_id
    element = self._redis.hget(REDIS_ELEMENT_MAP, element_id)

    if (timeout_queue == NULL)
    {
        // Retrieve element timeout
        timeout_queue = self._redis.hget(REDIS_QUEUE_MAP, element_id)
    }

    # Remove the element from this queue
    self._pipe.hdel(REDIS_QUEUE_MAP, element_id)
    self._pipe.hdel(REDIS_ELEMENT_MAP, element_id)
    self._pipe.hdel(REDIS_EXPIRATION_MAP, element_id)
    self._pipe.lrem(timeout_queue, element_id)
    self.unset_element_dehydrating(element_id)
    return element

}

/*
* dehydrator.pull <element_id>
* Pull an element off the bench by id.
*/
int PullCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 2)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);
    return _pull(ctx, argv[1], NULL)
}


RedisModuleString* _inspect(RedisModuleCtx *ctx, RedisModuleString * element_id, RedisModuleString * timeout_queue)
{
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "HGET", "cs", REDIS_EXPIRATION_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(srep);
    expiration = RedisModule_CallReplyInteger(srep)
    time_t result = time(NULL);
    // if(result != -1) //TODO: this seatbelt may be needed
    now = (uintmax_t)result;
    if expiration and  (expiration <= now):
        return _pull(element_id, timeout_queue)
    return NULL
}

//
// int _queue_to_int(char* queue_name)
// {
//     sscanf(queue_name, REDIS_QUEUE_NAME_FORMAT, &num_str)
//     return num_str
// }

/*
* dehydrator.poll
* get all elements which were dried for long enogh
*/
int PollCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) //TODO: this should return a list of elements
{
    if (argc != 1)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    expired_element_num = 0
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "KEYS", "c", REDIS_QUEUE_NAME_FORMAT_PATTERN);
    RMUTIL_ASSERT_NOERROR(rep);
    size_t timeout_num = RedisModule_CallReplyLength(rep);
    timeouts_reply = REDISMODULE_REPLY_ARRAY(rep); //TODO: this line is incorrect
    RedisModuleString** next_timeouts = (RedisModuleString**)(malloc(timeout_num * sizeof(RedisModuleString*)));
    for (i=0; i<timeout_num; i++)
    {
        timeouts[i] = RedisModule_CallReplyArrayElement(timeouts_reply, i);
    }

    while (timeout_num > 0)
    {
        RedisModuleString* next_timeouts[timeout_num] = {NULL}
        size_t next_timeouts_num = 0
        //Pull next item for all timeouts (effeciently)
        for (i=0; i<timeout_num; i++)
        {
            RedisModuleString * timeout_queue = timeouts[i]
            RedisModuleCallReply *srep =
                RedisModule_Call(ctx, "LPOP", "s", timeout_queue);
            RMUTIL_ASSERT_NOERROR(srep);

            RedisModuleString * element_id = RedisModule_CallReplyStringPtr(srep);
            RedisModuleString *element = _inspect(element_id, timeout_queue);

            if (element != NULL)
            {
                // element was rehydrated, return again to this queue to see if
                // there are more rehydratable elements
                RedisModule_ReplyWithString(ctx, element);
                expired_element_num++;

                timeouts[next_timeouts_num] = timeout_queue
                next_timeouts_num++;
            }
            else
            {
                // this element needs to dehydrate longer, push it back
                // to the front of the queue
                RedisModuleCallReply *srep =
                    RedisModule_Call(ctx, "LPUSH", "ss", timeout_queue, element_id);
                RMUTIL_ASSERT_NOERROR(srep);
            }
        }

        timeout_num = next_timeouts_num
    }

    RedisModule_ReplySetArrayLength(ctx, expired_element_num);
    free(next_timeouts);
    return REDISMODULE_OK;
}

// test the HGETSET command
int testHgetSet(RedisModuleCtx *ctx) {
  RedisModuleCallReply *r =
      RedisModule_Call(ctx, "example.hgetset", "ccc", "foo", "bar", "baz");
  RMUtil_Assert(RedisModule_CallReplyType(r) != REDISMODULE_REPLY_ERROR);

  r = RedisModule_Call(ctx, "example.hgetset", "ccc", "foo", "bar", "bag");
  RMUtil_Assert(RedisModule_CallReplyType(r) == REDISMODULE_REPLY_STRING);
  RMUtil_AssertReplyEquals(r, "baz");
  r = RedisModule_Call(ctx, "example.hgetset", "ccc", "foo", "bar", "bang");
  RMUtil_AssertReplyEquals(r, "bag");
  return REDISMODULE_OK;
}

// Unit test entry point for the module
int TestModule(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  RedisModule_AutoMemory(ctx);

  RMUtil_Test(testParse);
  RMUtil_Test(testHgetSet);

  RedisModule_ReplyWithSimpleString(ctx, "PASS");
  return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {

  // Register the module itself
  if (RedisModule_Init(ctx, "dehydrator", 1, REDISMODULE_APIVER_1) ==
      REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  // registerdehydrator.push - using the shortened utility registration macro
  RMUtil_RegisterWriteCmd(ctx, "dehydrator.push", PushCommand);

  // registerdehydrator.pull - using the shortened utility registration macro
  RMUtil_RegisterWriteCmd(ctx, "dehydrator.pull", PullCommand);

  // registerdehydrator.poll - using the shortened utility registration macro
  RMUtil_RegisterWriteCmd(ctx, "dehydrator.poll", PollCommand);


  // register the unit test
  RMUtil_RegisterWriteCmd(ctx, "dehydrator.test", TestModule);

  return REDISMODULE_OK;
}
