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
#define REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT_PATTERN "*:element_dehydrating"

void printRedisStr(RedisModuleString *str, const char* name) {
    const char *c;
    if (str == NULL)
    {
        c = NULL;
    }
    else
    {
        size_t len;
        c = RedisModule_StringPtrLen(str, &len);
    }
    printf("%s = %s\n", name, c);
}

void* OpenKeyC(RedisModuleCtx *ctx, char* key_name, int mode)
{
    RedisModuleString * key_name_str = RedisModule_CreateStringPrintf(ctx, key_name);
    RedisModuleKey * key = RedisModule_OpenKey(ctx, key_name_str, mode);
    RedisModule_FreeString(ctx, key_name_str);
    return key;
}
RedisModuleString * _format_single_redis_module_string(RedisModuleCtx *ctx, const char* format, RedisModuleString *str)
{
    const char *c;
    size_t len;
    c = RedisModule_StringPtrLen(str, &len);
    return RMUtil_CreateFormattedString(ctx, format, c);
}


bool _test_is_element_dehydrating_now(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    RedisModuleKey * element_dehydrating_key =
        RedisModule_OpenKey(
            ctx,
            _format_single_redis_module_string(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, element_id),
            REDISMODULE_READ
        );
    bool retval = (element_dehydrating_key != NULL);
    RedisModule_CloseKey(element_dehydrating_key);
    return retval;
}


void _set_element_dehydrating(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    RedisModuleString * element_dehydrating_key =
        _format_single_redis_module_string(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, element_id);
    RedisModule_Call(ctx, "SET", "sl", element_dehydrating_key, 1);
}


void _unset_element_dehydrating(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    RedisModuleString * element_dehydrating_key =
        _format_single_redis_module_string(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, element_id);
    RedisModule_Call(ctx, "DEL", "s", element_dehydrating_key);
}


void clear(RedisModuleCtx *ctx)
{
    // CAREFUL! (for testing purposes only)
    RedisModule_Call(ctx, "FLUSHALL", "");
}


bool _test_set_is_element_dehydrating_now(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    if (_test_is_element_dehydrating_now(ctx ,element_id))
        return true;
    _set_element_dehydrating(ctx, element_id);
    return false;
}


int ClearDehydratorCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 1)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);

    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "KEYS", "c", REDIS_QUEUE_NAME_FORMAT_PATTERN);
    size_t timeout_num = RedisModule_CallReplyLength(rep);
    for (int i=0; i<timeout_num; i++)
    {

        RedisModuleCallReply *timeouts_reply = RedisModule_CallReplyArrayElement(rep, i);
        RedisModuleString * timeout_queue = RedisModule_CreateStringFromCallReply(timeouts_reply);
        RedisModule_Call(ctx, "DEL", "s", timeout_queue);
    }

    RedisModule_Call(ctx, "DEL", "c", REDIS_QUEUE_MAP);
    RedisModule_Call(ctx, "DEL", "c", REDIS_ELEMENT_MAP);
    RedisModule_Call(ctx, "DEL", "c", REDIS_EXPIRATION_MAP);

    RedisModuleCallReply *rep2 =
        RedisModule_Call(ctx, "KEYS", "c", REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT_PATTERN);
    size_t element_num = RedisModule_CallReplyLength(rep2);

    for (int i=0; i<element_num; i++)
    {

        RedisModuleCallReply *element_reply = RedisModule_CallReplyArrayElement(rep2, i);
        RedisModuleString * element = RedisModule_CreateStringFromCallReply(element_reply);
        RedisModule_Call(ctx, "DEL", "s", element);
    }
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}


int IsDehydratingCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 2)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);

    RedisModuleString *element = NULL;
    int rep = REDISMODULE_OK;
    RedisModuleKey* REDIS_ELEMENT_MAP_key = OpenKeyC(ctx, REDIS_ELEMENT_MAP, REDISMODULE_READ);
    if (REDIS_ELEMENT_MAP_key != NULL)
    {
        rep = RedisModule_HashGet(REDIS_ELEMENT_MAP_key, REDISMODULE_HASH_NONE, argv[1], &element, NULL);
    }
    RedisModule_CloseKey(REDIS_ELEMENT_MAP_key);

    if (rep != REDISMODULE_ERR && element != NULL)
    {
        RedisModule_ReplyWithString(ctx, element);
        return REDISMODULE_OK;
    }

    RedisModule_ReplyWithNull(ctx);
    return rep;
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

    // make sure we have the queue listed
    RedisModule_Call(ctx, "HSET", "css", REDIS_QUEUE_MAP, element_id, dehydration_queue_name);

    // add the element to the dehydrating elements map
    RedisModule_Call(ctx, "HSET", "css", REDIS_ELEMENT_MAP, element_id, element);


    time_t result = time(NULL);
    long long timeout_int;
    RedisModule_StringToLongLong(argv[3], &timeout_int);
    long long expiration = (uintmax_t)result + timeout_int;

    // add the element to the element expiration hash
    char expiration_buffer[256];
    sprintf(expiration_buffer, "%lld", expiration);
    RedisModule_Call(ctx, "HSETNX", "csc", REDIS_EXPIRATION_MAP, element_id, expiration_buffer);

    // add the elemet id to the actual dehydration queue
    RedisModule_Call(ctx, "RPUSH", "ss", dehydration_queue_name, element_id);

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}


RedisModuleString * _pull(RedisModuleCtx *ctx, RedisModuleString * element_id, RedisModuleString * timeout_queue_var)
{

    RedisModuleString *timeout_queue = timeout_queue_var;
    RedisModuleKey* REDIS_QUEUE_MAP_key = OpenKeyC(ctx, REDIS_QUEUE_MAP, REDISMODULE_READ | REDISMODULE_WRITE);
    if (timeout_queue == NULL)
    {
        // Retrieve element timeout
        RedisModule_HashGet(REDIS_QUEUE_MAP_key, REDISMODULE_HASH_NONE, element_id, &timeout_queue, NULL);
        if (timeout_queue == NULL)
        {
            return NULL;
        }
    }

    RedisModuleString *element;
    RedisModuleKey* REDIS_ELEMENT_MAP_key = OpenKeyC(ctx, REDIS_ELEMENT_MAP, REDISMODULE_READ | REDISMODULE_WRITE);
    RedisModule_HashGet(REDIS_ELEMENT_MAP_key, REDISMODULE_HASH_NONE, element_id, &element, NULL);

    // Remove the element from this queue
    RedisModule_HashSet(REDIS_ELEMENT_MAP_key, REDISMODULE_HASH_NONE, element_id, REDISMODULE_HASH_DELETE, NULL);
    RedisModule_HashSet(REDIS_QUEUE_MAP_key, REDISMODULE_HASH_NONE, element_id, REDISMODULE_HASH_DELETE, NULL);
    RedisModule_Call(ctx, "HDEL", "cs", REDIS_EXPIRATION_MAP, element_id);
    RedisModule_Call(ctx, "LREM", "sls", timeout_queue, 1, element_id);

    _unset_element_dehydrating(ctx, element_id);
    RedisModule_CloseKey(REDIS_QUEUE_MAP_key);
    RedisModule_CloseKey(REDIS_ELEMENT_MAP_key);
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
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_ERR;
    }
    RedisModule_ReplyWithString(ctx, element);
    return REDISMODULE_OK;
}


RedisModuleString * _inspect(RedisModuleCtx *ctx, RedisModuleString * element_id, RedisModuleString * timeout_queue)
{
    RedisModuleString * expiration_str;
    RedisModuleKey* REDIS_EXPIRATION_MAP_key = OpenKeyC(ctx, REDIS_EXPIRATION_MAP, REDISMODULE_READ);
    RedisModule_HashGet(REDIS_EXPIRATION_MAP_key, REDISMODULE_HASH_NONE, element_id, &expiration_str, NULL);
    RedisModule_CloseKey(REDIS_EXPIRATION_MAP_key);
    long long expiration;
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
    size_t timeout_num = RedisModule_CallReplyLength(rep);
    for (int i=0; i<timeout_num; i++)
    {

        RedisModuleCallReply *timeouts_reply = RedisModule_CallReplyArrayElement(rep, i);
        RedisModuleString * timeout_queue = RedisModule_CreateStringFromCallReply(timeouts_reply);
        RedisModuleKey * timeout_queue_key =
            RedisModule_OpenKey(ctx, timeout_queue, REDISMODULE_READ | REDISMODULE_WRITE);
        bool should_pop_next = true;
        while (should_pop_next)
        {
            RedisModuleString * element_id = RedisModule_ListPop(timeout_queue_key , REDISMODULE_LIST_HEAD);
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
                    RedisModule_ListPush(timeout_queue_key , REDISMODULE_LIST_HEAD, element_id);
                    should_pop_next = false;
                }
            }
            else
            {
                should_pop_next = false;
            }
        }
        RedisModule_CloseKey(timeout_queue_key);
    }

    RedisModule_ReplySetArrayLength(ctx, expired_element_num);
    return REDISMODULE_OK;
}


int TestLook(RedisModuleCtx *ctx)
{
    RedisModule_Call(ctx, "dehydrator.clear", "");
    printf("Testing Look - ");

    // size_t len
    RedisModuleCallReply *check1 =
        RedisModule_Call(ctx, "dehydrator.look", "c", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(check1) != REDISMODULE_REPLY_ERROR);
    // check if X is dehydtaring (should be false)
    RMUtil_Assert(RedisModule_CreateStringFromCallReply(check1) == NULL);


    RedisModuleCallReply *push1 =
        RedisModule_Call(ctx, "dehydrator.push", "ccc", "test_element", "payload", "100");
    RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check2 =
        RedisModule_Call(ctx, "dehydrator.look", "c", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(check2) != REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(RedisModule_CreateStringFromCallReply(check2) != NULL); //TODO: == "payload"


    RedisModuleCallReply *pull1 =
        RedisModule_Call(ctx, "dehydrator.pull", "c", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(pull1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check3 =
        RedisModule_Call(ctx, "dehydrator.look", "c", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(check3) != REDISMODULE_REPLY_ERROR);
    // check if X is dehydtaring (should be false)
    RMUtil_Assert(RedisModule_CreateStringFromCallReply(check3) == NULL);

    RedisModule_Call(ctx, "dehydrator.clear", "");;
    printf("Passed.\n");
    return REDISMODULE_OK;
}


int TestPush(RedisModuleCtx *ctx)
{
    RedisModule_Call(ctx, "dehydrator.clear", "");;
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

    RedisModule_Call(ctx, "dehydrator.clear", "");;
    printf("Passed.\n");
    return REDISMODULE_OK;
}


int TestPull(RedisModuleCtx *ctx)
{
    RedisModule_Call(ctx, "dehydrator.clear", "");;
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
    RedisModule_Call(ctx, "dehydrator.clear", "");;
    return REDISMODULE_OK;
}


int TestPoll(RedisModuleCtx *ctx)
{
    printf("Testing Poll - ");

  // clear dehydrator
  RedisModule_Call(ctx, "dehydrator.clear", "");;

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
  RedisModule_Call(ctx, "dehydrator.clear", "");
  printf("Passed.\n");
  return REDISMODULE_OK;
}


// Unit test entry point for the module
int TestModule(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);

    RMUtil_Test(TestLook);
    RMUtil_Test(TestPush);
    RMUtil_Test(TestPull);
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

    // register dehydrator.clear - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.clear", ClearDehydratorCommand);

    // register dehydrator.push - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.push", PushCommand);

    // register dehydrator.pull - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.pull", PullCommand);

    // register dehydrator.pop - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.pop", PullCommand);

    // register dehydrator.poll - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.poll", PollCommand);

    // register dehydrator.look - using the shortened utility registration macro
    RMUtil_RegisterReadCmd(ctx, "dehydrator.look", IsDehydratingCommand);

    // register the unit test
    RMUtil_RegisterWriteCmd(ctx, "dehydrator.test", TestModule);

    return REDISMODULE_OK;
}
