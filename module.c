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

// Format of the dehydration flag (for sanity)
#define REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT "%s:element_dehydrating"

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
    return rep
}

bool _is_dehydrating(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    RedisModuleCallReply * response = _test_is_element_dehydrating_now(ctx, argv[1]);
    return (RedisModule_CallReplyInteger(response) == 1)
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

void clear(self)
{
    // CAREFUL! (for testing purposes only)
    RedisModuleString * element_dehydrating_key =
        _format_single_redis_module_string(ctx, REDIS_SET_DEHYDRATED_ELEMENTS_FORMAT, element_id);
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "FLUSHALL");
    RMUTIL_ASSERT_NOERROR(rep);
}

bool _test_set_is_element_dehydrating_now(RedisModuleCtx *ctx, RedisModuleString *element_id)
{
    if (_is_dehydrating(element_id))
        return true
    _set_element_dehydrating(element_id)
    return false

}

int IsDehydratingCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 2)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);
    RedisModuleCallReply * response = _test_is_element_dehydrating_now(ctx, argv[1]);
    RedisModule_ReplyWithCallReply(ctx, response)
    return REDISMODULE_OK;
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

    if (_test_set_is_element_dehydrating_now(element_id))
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
    RedisModuleString * dehydration_queue_name = _format_single_redis_module_string(ctx, REDIS_QUEUE_NAME_FORMAT, timeout);
    RedisModuleCallReply *srep =
        RedisModule_Call(ctx, "RPUSH", "ss", dehydration_queue_name, element_id);
    RMUTIL_ASSERT_NOERROR(srep);

    return REDISMODULE_OK;
}


RedisModuleString* _pull(RedisModuleCtx *ctx, RedisModuleString * element_id, RedisModuleString * timeout_queue)
{

    RedisModuleString *element = NULL
    // pulling out from dehydrator element <element_id>
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "HGET", "cs", REDIS_ELEMENT_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(rep);
    RedisModuleString *element = RedisModule_CreateStringFromCallReply(rep)

    if (timeout_queue == NULL)
    {
        // Retrieve element timeout
        RedisModuleCallReply *qrep =
            RedisModule_Call(ctx, "HGET", "cs", REDIS_QUEUE_MAP, element_id);
        RMUTIL_ASSERT_NOERROR(qrep);
        RedisModuleString *timeout_queue = RedisModule_CreateStringFromCallReply(qrep)
    }

    // Remove the element from this queue
    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "HDEL", "cs", REDIS_ELEMENT_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(rep);

    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "HDEL", "cs", REDIS_QUEUE_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(rep);

    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "HDEL", "cs", REDIS_EXPIRATION_MAP, element_id);
    RMUTIL_ASSERT_NOERROR(rep);

    RedisModuleCallReply *rep =
        RedisModule_Call(ctx, "LREM", "cs", timeout_queue, element_id);
    RMUTIL_ASSERT_NOERROR(rep);

    _unset_element_dehydrating(element_id)
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
    RedisModuleString * element = _pull(ctx, argv[1], NULL);
    RedisModule_ReplyWithString(ctx, element);
    return REDISMODULE_OK;
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
int PollCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
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
    for (i=0; i<timeout_num; i++)
    {

        RedisModuleCallReply *timeouts_reply = RedisModule_CallReplyArrayElement(rep, i);
        RedisModuleString * timeout_queue =
        bool should_pop_next = true;
        while (should_pop_next)
        {
            RedisModuleCallReply *sub_rep =
                RedisModule_Call(ctx, "LPOP", "s", timeout_queue);
            RMUTIL_ASSERT_NOERROR(sub_rep);

            RedisModuleString * element_id = RedisModule_CreateStringFromCallReply(sub_rep);
            if (element_id != NULL)
            {
                RedisModuleString * element = _inspect(element_id, timeout_queue);

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


int TestIsDehydrating(RedisModuleCtx *ctx)
{
    // TODO: stub; write me
    return REDISMODULE_OK;
}


int TestPush(RedisModuleCtx *ctx)
{
    // TODO: stub; write me
    return REDISMODULE_OK;
}


int TestPull(RedisModuleCtx *ctx)
{
    // TODO: stub; write me
    return REDISMODULE_OK;
}


int TestPoll(RedisModuleCtx *ctx)
{
    // def test_dehydrator():
    //     d = RedisDehydratorService()
    //     d.clear()
    //     q = DummyCallback()
    //     d._callback_queue = q
    //
    //     print "push questions 1,7 & 3a (for 1,7 & 3 seconds)"
    //     d.push(DummyQuestion("test_question1"),1)
    //     d.push(DummyQuestion("test_question7"),7)
    //     d.push(DummyQuestion("test_question3a"),3)
    //     print "pull question 7"
    //     d.pull("test_question7")
    //     print "poll - make sure no question pops right out"
    //     d.poll()
    //     assert(len(q.q) == 0)
    //     print "sleep 1"
    //     time.sleep(1)
    //     print "push question 3b (for 3 seconds)"
    //     d.push(DummyQuestion("test_question3b"),3)
    //     print "poll (t=1) - we expect only question 1 to pop out"
    //     d.poll()
    //     print str(q.q)
    //     assert(len(q.q) == 1)
    //     assert(q.q[0] == "test_question1")
    //     print "sleep 2 and poll (t=3) - we expect question 3a to pop out"
    //     time.sleep(2)
    //     d.poll()
    //     assert(len(q.q) == 2)
    //     assert(q.q[1] == "test_question3a")
    //     print "sleep 2 and poll (t=5) - we expect question 3b to pop out"
    //     time.sleep(2)
    //     d.poll()
    //     assert(len(q.q) == 3)
    //     assert(q.q[2] == "test_question3b")
    //     print "sleep 6"
    //     time.sleep(6)
    //     print "poll (t=11) - we expect that question 7 will NOT pop out, because we already pulled it"
    //     d.poll()
    //     assert(len(q.q) == 3)
    //     d.clear()
}


// Unit test entry point for the module
int TestModule(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
  RedisModule_AutoMemory(ctx);

  RMUtil_Test(TestPush);
  RMUtil_Test(TestIsDehydrating);
  RMUtil_Test(TestPull);
  RMUtil_Test(TestPoll);

  RedisModule_ReplyWithSimpleString(ctx, "PASS");
  return REDISMODULE_OK;
}


int RedisModule_OnLoad(RedisModuleCtx *ctx) {
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
