#include "redismodule.h"
#include "rmutil/util.h"
#include "rmutil/strings.h"
#include "rmutil/test_util.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

//Name of a hash-map mapping element IDs to queue timeouts (integers)
#define REDIS_QUEUE_MAP "element_queues"

//Name of a hash-map mapping element IDs to elements (JSON strings)
#define REDIS_ELEMENT_MAP "element_objects"

// Name of a hash-map mapping element IDs to elements (JSON strings)
#define REDIS_EXPIRATION_MAP "element_expiration"

// Format of the queue name (for storage)
#define REDIS_QUEUE_NAME_FORMAT "timeout_queue#%s"
#define REDIS_QUEUE_NAME_FORMAT_PATTERN "timeout_queue#*"

// Format of the dehydration flag (for sanity)
#define REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT "%s:element_dehydrating"

void printRedisStr(RedisModuleString *str, const char* name) {
    const char *c;
    size_t len;
    c = RedisModule_StringPtrLen(str, &len);
    printf("%s = %s\n", name, c);
}

RedisModuleString * _format_single_redis_module_string(RedisModuleCtx *ctx, const char* format, RedisModuleString *str)
{
    const char *c;
    size_t len;
    c = RedisModule_StringPtrLen(str, &len);
    return RMUtil_CreateFormattedString(ctx, format, c);
}


RedisModuleCallReply * _test_is_element_dehydrating_now(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    RedisModuleString * element_dehydrating_key =
        _format_single_redis_module_string(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, element_id);
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "EXISTS", "s", element_dehydrating_key);
    RMUTIL_ASSERT_NOERROR(rep);
    return rep;
}


bool _is_dehydrating(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    RedisModuleCallReply * response = _test_is_element_dehydrating_now(ctx, element_id);
    return (RedisModule_CallReplyInteger(response) == 1);
}


void _set_element_dehydrating(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    RedisModuleString * element_dehydrating_key =
        _format_single_redis_module_string(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, element_id);
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "SET", "sl", element_dehydrating_key, 1);
    RMUTIL_ASSERT_NOERROR(rep);
}


void _unset_element_dehydrating(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    RedisModuleString * element_dehydrating_key =
        _format_single_redis_module_string(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, element_id);
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "DEL", "s", element_dehydrating_key);
    RMUTIL_ASSERT_NOERROR(rep);
}


void clear(RedisModuleCtx *ctx)
{
    // CAREFUL! (for testing purposes only)
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "FLUSHALL", "");
    RMUTIL_ASSERT_NOERROR(rep);
}


bool _test_set_is_element_dehydrating_now(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    if (_is_dehydrating(ctx ,element_id))
        return true;
    _set_element_dehydrating(ctx, element_id);
    return false;
}


int IsDehydratingCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 2)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);
    RedisModuleCallReply * response = _test_is_element_dehydrating_now(ctx, argv[1]);
    RedisModule_ReplyWithCallReply(ctx, response);
    return REDISMODULE_OK;
}


/*
* dehydrator.push <element_id> <element> <timeout>
* dehydrate <element> for <timeout> seconds
*/
int PushCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    // we need EXACTLY 4 arguments
    if (argc != 4)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);

    RedisModuleString * element_id = argv[1];
    RedisModuleString * element = argv[2];
    RedisModuleString * timeout = argv[3];

    if (_test_set_is_element_dehydrating_now(ctx, element_id))
    {
        RedisModule_ReplyWithError(ctx, "ERROR: Element already dehydrating.");
        return REDISMODULE_ERR;
    }

    RedisModuleString * dehydration_queue_name = _format_single_redis_module_string(ctx, REDIS_QUEUE_NAME_FORMAT, timeout);
    const char *tqc;
    size_t len;
    tqc = RedisModule_StringPtrLen(dehydration_queue_name, &len);

    // make sure we have the queue listed
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "HSET", "css", REDIS_QUEUE_MAP, element_id, dehydration_queue_name);
    RMUTIL_ASSERT_NOERROR(srep);

    // add the element to the dehydrating elements map
    srep = RedisModule_Call(ctx, "HSET", "css", REDIS_ELEMENT_MAP, element_id, element);
    RMUTIL_ASSERT_NOERROR(srep);


    time_t result = time(NULL);
    long long timeout_int;
    RedisModule_StringToLongLong(argv[3], &timeout_int);
    long long expiration = (uintmax_t)result + timeout_int;

    // add the element to the element expiration hash
    char buffer[256];
    sprintf(buffer, "%lld", expiration);
    srep = RedisModule_Call(ctx, "HSETNX", "csc", REDIS_EXPIRATION_MAP, element_id, buffer);
    RMUTIL_ASSERT_NOERROR(srep);

    // add the elemet id to the actual dehydration queue
    srep = RedisModule_Call(ctx, "RPUSH", "ss", dehydration_queue_name, element_id);
    RMUTIL_ASSERT_NOERROR(srep);

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}


RedisModuleString * _pull(RedisModuleCtx *ctx, RedisModuleString * element_id, RedisModuleString * timeout_queue_var)
{

    RedisModuleString *element = NULL;
    RedisModuleString *timeout_queue = timeout_queue_var;
    // pulling out from dehydrator element <element_id>
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "HGET", "cs", REDIS_ELEMENT_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(rep);
    element = RedisModule_CreateStringFromCallReply(rep);

    if (timeout_queue == NULL)
    {
        // Retrieve element timeout
        RedisModuleCallReply *qrep =
            RedisModule_Call(ctx, "HGET", "cs", REDIS_QUEUE_MAP, element_id);
        RMUTIL_ASSERT_NOERROR(qrep);
        timeout_queue = RedisModule_CreateStringFromCallReply(qrep);
        if (timeout_queue == NULL)
        {
            return NULL;
        }
        const char *timeout_queue_c;
        size_t len;
        timeout_queue_c = RedisModule_StringPtrLen(timeout_queue, &len);
    }

    // Remove the element from this queue
    rep = RedisModule_Call(ctx, "HDEL", "cs", REDIS_ELEMENT_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(rep);

    rep = RedisModule_Call(ctx, "HDEL", "cs", REDIS_QUEUE_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(rep);

    rep = RedisModule_Call(ctx, "HDEL", "cs", REDIS_EXPIRATION_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(rep);

    rep = RedisModule_Call(ctx, "LREM", "sls", timeout_queue, 1, element_id);
    RMUTIL_ASSERT_NOERROR(rep);

    _unset_element_dehydrating(ctx, element_id);
    return element;
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
    RedisModuleString * element = _pull(ctx, argv[1], NULL);
    if (element == NULL)
    {
        return REDISMODULE_ERR;
    }
    RedisModule_ReplyWithString(ctx, element);
    return REDISMODULE_OK;
}


RedisModuleString * _inspect(RedisModuleCtx *ctx, RedisModuleString * element_id, RedisModuleString * timeout_queue)
{
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "HGET", "cs", REDIS_EXPIRATION_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(srep);
    long long expiration;
    char* expiration_str = RedisModule_CreateStringFromCallReply(srep);
    RedisModule_StringToLongLong(expiration_str, &expiration);
    time_t result = time(NULL);
    uintmax_t now = (uintmax_t)result;
    if (expiration > 0 && (expiration <= now))
    {
        return _pull(ctx, element_id, timeout_queue);
    }
    return NULL;
}


/*
* dehydrator.poll
* get all elements which were dried for long enogh
*/
int PollCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 1)
    {
      return RedisModule_WrongArity(ctx);
    }

    RedisModule_AutoMemory(ctx);
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    int expired_element_num = 0;
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "KEYS", "c", REDIS_QUEUE_NAME_FORMAT_PATTERN);
    RMUTIL_ASSERT_NOERROR(rep);
    size_t timeout_num = RedisModule_CallReplyLength(rep);
    for (int i=0; i<timeout_num; i++)
    {

        RedisModuleCallReply *timeouts_reply = RedisModule_CallReplyArrayElement(rep, i);
        RedisModuleString * timeout_queue = RedisModule_CreateStringFromCallReply(timeouts_reply);
        bool should_pop_next = true;
        while (should_pop_next)
        {
            RedisModuleCallReply *sub_rep =
                RedisModule_Call(ctx, "LPOP", "s", timeout_queue);
            RMUTIL_ASSERT_NOERROR(sub_rep);

            RedisModuleString * element_id = RedisModule_CreateStringFromCallReply(sub_rep);
            if (element_id != NULL)
            {
                RedisModuleString * element = _inspect(ctx, element_id, timeout_queue);
                if (element != NULL)
                {
                    // element was rehydrated, return again to this queue to see if
                    // there are more rehydratable elements
                    RedisModule_ReplyWithString(ctx, element);
                    expired_element_num++;
                }
                else
                {
                    // this element needs to dehydrate longer, push it back
                    // to the front of the queue
                    RedisModuleCallReply *srep =
                        RedisModule_Call(ctx, "LPUSH", "ss", timeout_queue, element_id);
                    RMUTIL_ASSERT_NOERROR(srep);
                    should_pop_next = false;
                }
            }
            else
            {
                should_pop_next = false;
            }
        }
    }

    RedisModule_ReplySetArrayLength(ctx, expired_element_num);
    return REDISMODULE_OK;
}


int TestIsDehydrating(RedisModuleCtx *ctx)
{
    size_t len;
    clear(ctx);
    printf("Testing IsDehydrating - ");

    RedisModuleCallReply *check1 =
        RedisModule_Call(ctx, "dehydrator.isDehydrating", "c", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(check1) != REDISMODULE_REPLY_ERROR);
    // check if X is dehydtaring (should be false)
    RMUtil_Assert(RedisModule_CallReplyInteger(check1) == 0);


    RedisModuleCallReply *push1 =
        RedisModule_Call(ctx, "dehydrator.push", "ccc", "test_element", "payload", "100");
    RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check2 =
        RedisModule_Call(ctx, "dehydrator.isDehydrating", "c", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(check2) != REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(RedisModule_CallReplyInteger(check2) == 1);


    RedisModuleCallReply *pull1 =
        RedisModule_Call(ctx, "dehydrator.pull", "c", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(pull1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check3 =
        RedisModule_Call(ctx, "dehydrator.isDehydrating", "c", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(check3) != REDISMODULE_REPLY_ERROR);
    // check if X is dehydtaring (should be false)
    RMUtil_Assert(RedisModule_CallReplyInteger(check1) == 0);

    clear(ctx);
    printf("Passed.\n");
    return REDISMODULE_OK;
}


int TestPush(RedisModuleCtx *ctx)
{
    clear(ctx);
    printf("Testing Push - ");
    // char * element_id = "push_test_element";
    RedisModuleCallReply *push1 =
        RedisModule_Call(ctx, "dehydrator.push", "ccc", "push_test_element", "payload", "1");
    RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);

    RedisModuleString * store_key = RMUtil_CreateFormattedString(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, "push_test_element");

    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "EXISTS", "s", store_key);
    RMUTIL_ASSERT_NOERROR(rep);
    RMUtil_Assert(RedisModule_CallReplyInteger(rep) == 1);

    // TODO: add fail-case tests

    clear(ctx);
    printf("Passed.\n");
    return REDISMODULE_OK;
}


int TestPull(RedisModuleCtx *ctx)
{
    clear(ctx);
    printf("Testing Pull - ");

    RedisModuleString * store_key = RMUtil_CreateFormattedString(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, "pull_test_element");
    RedisModuleString * bad_store_key = RMUtil_CreateFormattedString(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, "pull_test_bad_element");

    RedisModuleCallReply *push1 =
        RedisModule_Call(ctx, "dehydrator.push", "ccc", "pull_test_element", "payload", "100");
    RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);


    RedisModuleCallReply *rep1 =
        RedisModule_Call(ctx, "EXISTS", "s", store_key);
    RMUTIL_ASSERT_NOERROR(rep1);
    RMUtil_Assert(RedisModule_CallReplyInteger(rep1) == 1);


    RedisModuleCallReply *pull1 =
        RedisModule_Call(ctx, "dehydrator.pull", "c", "pull_test_bad_element");
    RMUtil_Assert(RedisModule_CallReplyType(pull1) != REDISMODULE_REPLY_ERROR);


    RedisModuleCallReply *rep2 =
        RedisModule_Call(ctx, "EXISTS", "s", bad_store_key);
    RMUTIL_ASSERT_NOERROR(rep2);
    RMUtil_Assert(RedisModule_CallReplyInteger(rep2) == 0);


    RedisModuleCallReply *pull2 =
        RedisModule_Call(ctx, "dehydrator.pull", "c", "pull_test_element");
    RMUtil_Assert(RedisModule_CallReplyType(pull2) != REDISMODULE_REPLY_ERROR);


    RedisModuleCallReply *rep3 =
        RedisModule_Call(ctx, "EXISTS", "s", store_key);
    RMUTIL_ASSERT_NOERROR(rep3);
    RMUtil_Assert(RedisModule_CallReplyInteger(rep3) == 0);

    printf("Passed.\n");
    clear(ctx);
    return REDISMODULE_OK;
}


int TestPoll(RedisModuleCtx *ctx)
{
    printf("Testing Poll - ");

  // clear dehydrator
  clear(ctx);

  // start test
  // push elements 1, 4, 7 & 3a (for 1, 4, 7 & 3 seconds)
  // 1
  RedisModuleCallReply *push1 =
      RedisModule_Call(ctx, "dehydrator.push", "ccc", "e1", "element_1", "1");
  RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);

  // 4
  RedisModuleCallReply *push4 =
      RedisModule_Call(ctx, "dehydrator.push", "ccc", "e4", "element_4", "4");
  RMUtil_Assert(RedisModule_CallReplyType(push4) != REDISMODULE_REPLY_ERROR);

  // 7
  RedisModuleCallReply *push7 =
      RedisModule_Call(ctx, "dehydrator.push", "ccc", "e7", "element_7", "7");
  RMUtil_Assert(RedisModule_CallReplyType(push7) != REDISMODULE_REPLY_ERROR);

  // 3a
  RedisModuleCallReply *push3a =
      RedisModule_Call(ctx, "dehydrator.push", "ccc", "e3a", "element_3a", "3");
  RMUtil_Assert(RedisModule_CallReplyType(push3a) != REDISMODULE_REPLY_ERROR);

  // pull question 7
  RedisModuleCallReply *pull_seven_rep =
      RedisModule_Call(ctx, "dehydrator.pull", "c", "e7");
  RMUtil_Assert(RedisModule_CallReplyType(pull_seven_rep) != REDISMODULE_REPLY_ERROR);

  // poll - make sure no element pops right out
  RedisModuleCallReply *poll1_rep =
      RedisModule_Call(ctx, "dehydrator.poll", "");
  RMUtil_Assert(RedisModule_CallReplyType(poll1_rep) != REDISMODULE_REPLY_ERROR);
  RMUtil_Assert(RedisModule_CallReplyLength(poll1_rep) == 0);

  // sleep 1 sec
  sleep(1);

  // push element 3b (for 3 seconds)
  // 3b
  RedisModuleCallReply *push_three_b =
      RedisModule_Call(ctx, "dehydrator.push", "ccc", "e3b", "element_3b", "3");
  RMUtil_Assert(RedisModule_CallReplyType(push_three_b) != REDISMODULE_REPLY_ERROR);

  // poll (t=1) - we expect only element 1 to pop out
  RedisModuleCallReply *poll_two_rep =
      RedisModule_Call(ctx, "dehydrator.poll", "");
  RMUtil_Assert(RedisModule_CallReplyType(poll_two_rep) != REDISMODULE_REPLY_ERROR);
  RMUtil_Assert(RedisModule_CallReplyLength(poll_two_rep) == 1);
  RedisModuleCallReply *subreply_a = RedisModule_CallReplyArrayElement(poll_two_rep, 0);
  RMUtil_AssertReplyEquals(subreply_a, "element_1")

  // sleep 2 secs and poll (t=3) - we expect only element 3a to pop out
  sleep(2);
  RedisModuleCallReply *poll_three_rep =
      RedisModule_Call(ctx, "dehydrator.poll", "");
  RMUtil_Assert(RedisModule_CallReplyType(poll_three_rep) != REDISMODULE_REPLY_ERROR);
  RMUtil_Assert(RedisModule_CallReplyLength(poll_three_rep) == 1);
  RedisModuleCallReply *subreply_b = RedisModule_CallReplyArrayElement(poll_three_rep, 0);
  RMUtil_AssertReplyEquals(subreply_b, "element_3a");


  // sleep 2 secs and poll (t=5) - we expect elements 4 and 3b to pop out
  sleep(2);
  RedisModuleCallReply *poll_four_rep =
      RedisModule_Call(ctx, "dehydrator.poll", "");
  RMUtil_Assert(RedisModule_CallReplyType(poll_four_rep) != REDISMODULE_REPLY_ERROR);
  RMUtil_Assert(RedisModule_CallReplyLength(poll_four_rep) == 2);
  RedisModuleCallReply *subreply_c_first = RedisModule_CallReplyArrayElement(poll_four_rep, 0);
  RedisModuleCallReply *subreply_c_second = RedisModule_CallReplyArrayElement(poll_four_rep, 1);
  RedisModuleString * first_str =  RedisModule_CreateStringFromCallReply(subreply_c_first);
  RedisModuleString * second_str = RedisModule_CreateStringFromCallReply(subreply_c_second);
  RedisModuleString * element_3b_str = RedisModule_CreateString(ctx, "element_3b", strlen("element_3b"));
  RedisModuleString * element_4_str = RedisModule_CreateString(ctx, "element_4", strlen("element_4"));
  RMUtil_Assert(
    (
        RMUtil_StringEquals(first_str, element_3b_str) &&
        RMUtil_StringEquals(second_str, element_4_str)
    ) ||
    (
        RMUtil_StringEquals(first_str, element_4_str) &&
        RMUtil_StringEquals(second_str, element_3b_str)
    )
  );

  // sleep 6 secs and poll (t=11) - we expect that element 7 will NOT pop out, because we already pulled it
  sleep(6);
  RedisModuleCallReply *poll_five_rep = RedisModule_Call(ctx, "dehydrator.poll", "");
  RMUtil_Assert(RedisModule_CallReplyType(poll_five_rep) != REDISMODULE_REPLY_ERROR);
  RMUtil_Assert(RedisModule_CallReplyLength(poll_five_rep) == 0);

  // clear dehydrator
  clear(ctx);
  printf("Passed.\n");
  return REDISMODULE_OK;
}


// Unit test entry point for the module
int TestModule(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);

    RMUtil_Test(TestPush);
    RMUtil_Test(TestPull);
    RMUtil_Test(TestIsDehydrating);
    RMUtil_Test(TestPoll);

    RedisModule_ReplyWithSimpleString(ctx, "PASS");
    return REDISMODULE_OK;
}


int RedisModule_OnLoad(RedisModuleCtx *ctx)
{
    // Register the module itself
    if (RedisModule_Init(ctx, "dehydrator", 1, REDISMODULE_APIVER_1) ==
      REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    // register dehydrator.push - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.push", PushCommand);

    // register dehydrator.pull - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.pull", PullCommand);

    // register dehydrator.poll - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.poll", PollCommand);

    // register dehydrator.isDehydrating - using the shortened utility registration macro
    RMUtil_RegisterReadCmd(ctx, "dehydrator.isDehydrating", IsDehydratingCommand);


    // register the unit test
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.test", TestModule);

    return REDISMODULE_OK;
}
