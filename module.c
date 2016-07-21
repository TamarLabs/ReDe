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

/*
* dehydrator.push <element_id> <element> <timeout>
* Atomically dehydrate <element> for <timeout> seconds
*
*/
int PushCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // we need EXACTLY 4 arguments  TODO: make sure what is in argv[0]
    if (argc != 4) {
      return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);
    /**
    Register the element to wait the timeout (in seconds),
    then add it to the queue.
    **/


    element_id = argv[1]
    element = argv[2]
    timeout = argv[3]


    // set the new value of the element
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "HSET", REDIS_QUEUE_MAP, element_id, timeout);
    RMUTIL_ASSERT_NOERROR(srep);

    // set the new value of the element
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "HSET", REDIS_ELEMENT_MAP, element_id, element);
    RMUTIL_ASSERT_NOERROR(srep);

    // set the new value of the element
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "HSETNX", REDIS_EXPIRATION_MAP, element_id, int(time.time() + timeout));
    RMUTIL_ASSERT_NOERROR(srep);

    time_t result = time(NULL);
    // if(result != -1) //TODO: this seatbelt may be needed
    expiration = (uintmax_t)result + timeout;

    // set the new value of the element
    char dehydration_queue_name[30];
    sprintf(dehydration_queue_name, REDIS_QUEUE_NAME_FORMAT, timeout);
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "RPUSH", , element_id, expiration);
    RMUTIL_ASSERT_NOERROR(srep);
    free(dehydration_queue_name);

    return 0;
}


int PollCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // we need EXACTLY 4 arguments  TODO: make sure what is in argv[0]
    if (argc != 4) {
      return RedisModule_WrongArity(ctx);
    }


    element_id = argv[1]
    element = argv[2]
    timeout = argv[3]



    RedisModule_AutoMemory(ctx);
    /**
    Register the element to wait the timeout (in seconds),
    then add it to the queue.
    **/


  // if the value was null before - we just return null
  if (RedisModule_CallReplyType(rep) == REDISMODULE_REPLY_NULL) {
    RedisModule_ReplyWithNull(ctx);
    return REDISMODULE_OK;
  }

  // forward the HGET reply to the client
  RedisModule_ReplyWithCallReply(ctx, rep);
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
  return 0;
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
  // RMUtil_RegisterWriteCmd(ctx, "dehydrator.pull", PullCommand);

  // registerdehydrator.poll - using the shortened utility registration macro
  RMUtil_RegisterWriteCmd(ctx, "dehydrator.poll", PollCommand);


  // register the unit test
  RMUtil_RegisterWriteCmd(ctx, "dehydrator.test", TestModule);

  return REDISMODULE_OK;
}
