#include "redismodule.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include "khash.h"
#include "rmutil/util.h"
#include "rmutil/strings.h"
#include "rmutil/test_util.h"
#include "rmutil/alloc.h"


//##########################################################
//#
//#                    C Utilities
//#
//#########################################################

#define ID_LENGTH 31
#define ALLOWED_ID_CHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

char* string_append(char* a, const char* b)
{
    char* retstr = RedisModule_Alloc(strlen(a)+strlen(b));
    strcpy(retstr, a);
    strcat(retstr, b);
    // printf("printing: %s", retstr);
    RedisModule_Free(a);
    return retstr;
}


long long current_time_ms (void)
{
    long            ms; // Milliseconds
    time_t          s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s  = spec.tv_sec*1000;
    ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds

    return s+ms;
}

// Assumes 0 <= max <= RAND_MAX
// Returns in the closed interval [0, max]
long random_at_most(long max) {
  unsigned long
    // max <= RAND_MAX < ULONG_MAX, so this is okay.
    num_bins = (unsigned long) max + 1,
    num_rand = (unsigned long) RAND_MAX + 1,
    bin_size = num_rand / num_bins,
    defect   = num_rand % num_bins;

  long x;
  do {
   x = random();
  }
  // This is carefully written not to overflow
  while (num_rand - defect <= (unsigned long)x);

  // Truncated division is intentional
  return x/bin_size;
}

char* generate_id(void)
{
    char* uuid = RedisModule_Alloc((ID_LENGTH+1)*sizeof(char));
    long allowed_char_range = strlen(ALLOWED_ID_CHARS);

    int i;
    for (i=0;i<ID_LENGTH;++i)
    {
        uuid[i] = ALLOWED_ID_CHARS[random_at_most(allowed_char_range)];
    }
    uuid[ID_LENGTH] = 0;
    return uuid;
}


//##########################################################
//#
//#               Linked List Definitions
//#
//#########################################################

typedef struct element_list_node{
    RedisModuleString* element;
    RedisModuleString* element_id;
    int ttl;
    long long expiration;
    struct element_list_node* next;
    struct element_list_node* prev;
} ElementListNode;

typedef struct element_list{
    ElementListNode* head;
    ElementListNode* tail;
    int len;
} ElementList;


//##########################################################
//#
//#                     Hash Maps
//#
//#########################################################

KHASH_MAP_INIT_INT(16, ElementList*);

KHASH_MAP_INIT_STR(32, ElementListNode*);


//##########################################################
//#
//#                     Type
//#
//#########################################################

static RedisModuleType *DehydratorType;

typedef struct dehydrator{
    khash_t(16) *timeout_queues; //<ttl,ElementList>
    khash_t(32) * element_nodes; //<element_id,node*>
    RedisModuleString* name;
} Dehydrator;


//##########################################################
//#
//#              Linked List Functions
//#
//#########################################################


//Creates a new Node and returns pointer to it.
ElementListNode* _createNewNode(RedisModuleString* element, RedisModuleString* element_id, long long ttl, long long expiration)
{
	ElementListNode* newNode
		= (ElementListNode*)RedisModule_Alloc(sizeof(ElementListNode));

    newNode->element_id = element_id;
    newNode->element = element;
	newNode->expiration = expiration;
	newNode->ttl = ttl;
	newNode->next = NULL;
    newNode->prev = NULL;
	return newNode;
}


void deleteNode(ElementListNode* node)
{
    // free everything else related to the node
    RedisModule_Free(node);
}


//Creates a new Node and returns pointer to it.
ElementList* _createNewList()
{
	ElementList* list
		= (ElementList*)RedisModule_Alloc(sizeof(ElementList));
	list->head = NULL;
	list->tail = NULL;
    list->len = 0;
	return list;
}


void deleteList(ElementList* list)
{
    ElementListNode* current = list->head;

    // iterate over queue and find the element that has id = element_id
    while(current != NULL)
    {
        ElementListNode* next = current->next; // save next
        deleteNode(current);
        current = next;  //move to next node
    }

    RedisModule_Free(list);
}


// insert a Node at tail of linked list
void _listPush(ElementList* list, ElementListNode* node)
{
	if (list->tail == NULL)
    {
		list->head = node;
	}
    else
    {
        node->prev = list->tail;
        list->tail->next = node;
    }
	list->tail = node;
    list->len = (list->len) + 1;
}


// pull and return the element at the first location
ElementListNode* _listPop(ElementList* list) {
   if ((list == NULL) || (list->head == NULL)) { return NULL; } // if list empty

   //save current head
   ElementListNode* node = list->head;

   if (list->len == 1)
   {
       list->tail = NULL;
       list->head = NULL;
   }
   else
   {
       // swap to new head
       list->head = list->head->next;
       list->head->prev = NULL;
   }

   list->len = list->len - 1;
   return node;

}


void _listPull(Dehydrator* dehydrator, ElementListNode* node)
{
	ElementList* list = NULL;
	khiter_t k = kh_get(16, dehydrator->timeout_queues, node->ttl);  // first have to get iterator
	if (k != kh_end(dehydrator->timeout_queues)) // k will be equal to kh_end if key not present
	{
		list = kh_val(dehydrator->timeout_queues, k);
	}
	if (list == NULL) { return; }

    if (list->len == 1)
    {
        list->head = NULL;
        list->tail = NULL;
        kh_del(16, dehydrator->timeout_queues, k);
        deleteList(list);
        return;
    }

    //hort circuit the node (carefull! pulling from tail or head)
    if (node == list->head)
    {
        list->head = list->head->next;
        list->head->prev = NULL;
    }
    else
    {
        node->prev->next = node->next;
    }

    if (node == list->tail) {
        list->tail = node->prev;
        list->tail->next = NULL;
    }
    else
    {
        node->next->prev = node->prev;
    }
    list->len = list->len - 1;
}

// pull from list and return an element with the following id
ElementListNode* _listFind(ElementList* list, RedisModuleString* element_id)
{
    //start from head
    ElementListNode* current = list->head;

    if (current == NULL) { return NULL; } //list is empty

    // iterate over queue and find the element that has id = element_id
    while (current->element_id != element_id)
    {
        if (current->next == NULL) { return NULL; } // got to tail
        current = current->next; //move to next node
    }

    while (current->element_id == element_id) // match found
    {
        return current;
    }

    return NULL;
}


char* printNode(ElementListNode* node)
{
    size_t element_id_len;
    size_t element_len;
    const char* element_id = RedisModule_StringPtrLen(node->element_id, &element_id_len);
    const char* element = RedisModule_StringPtrLen(node->element, &element_len);
    char* node_str = (char*)RedisModule_Alloc((element_id_len+element_len+50)*sizeof(char));
    sprintf(node_str, "[id=%s,elem=%s,ttl=%d,exp=%lld]", element_id, element, node->ttl, node->expiration);
    return node_str;

}


char* printList(ElementList* list)
{
    char* list_str = RedisModule_Alloc(32*sizeof(char));
    ElementListNode* current = list->head;
    sprintf(list_str, "(elements=%d)\n   head", list->len);
    // iterate over queue and find the element that has id = element_id
    while(current != NULL)
    {
        list_str = string_append(list_str, "->");
        char* node_str = printNode(current);
        list_str = string_append(list_str, node_str);
        RedisModule_Free(node_str);

        current = current->next;  //move to next node
    }
    list_str = string_append(list_str, "\n   tail points to: ");
    list_str = string_append(list_str, RedisModule_StringPtrLen(list->tail->element_id, NULL));
    list_str = string_append(list_str,"\n");
    return list_str;
}


//##########################################################
//#
//#               Dehydrator Utilities
//#
//#########################################################

Dehydrator* _createDehydrator(RedisModuleString* dehydrator_name)
{

    Dehydrator* dehy
		= (Dehydrator*)RedisModule_Alloc(sizeof(Dehydrator));

    dehy->timeout_queues = kh_init(16);
    dehy->element_nodes = kh_init(32);
	dehy->name = dehydrator_name;

    return dehy;
}


Dehydrator* validateDehydratorKey(RedisModuleCtx* ctx, RedisModuleKey* key, RedisModuleString* dehydrator_name)
{
	int type = RedisModule_KeyType(key);
	if (type != REDISMODULE_KEYTYPE_EMPTY &&
		RedisModule_ModuleTypeGetType(key) != DehydratorType)
	{
        RedisModule_ReplyWithError(ctx,REDISMODULE_ERRORMSG_WRONGTYPE);
        RedisModule_CloseKey(key);
		return NULL;
	}
	if (type == REDISMODULE_KEYTYPE_EMPTY)
    {
        if (dehydrator_name != NULL)
        {
            RedisModuleString* saved_dehydrator_name = RedisModule_CreateStringFromString(ctx, dehydrator_name);
            Dehydrator* dehydrator = _createDehydrator(saved_dehydrator_name);
            RedisModule_ModuleTypeSetValue(key, DehydratorType, dehydrator);
            return dehydrator;
        }
        else
        {
            RedisModule_CloseKey(key);
            return NULL;
        }
    }
	else
	{
		return RedisModule_ModuleTypeGetValue(key);
	}
}

char* printDehydrator(Dehydrator* dehydrator)
{
    char* dehy_str = RedisModule_Alloc(sizeof(char));
    khiter_t k;

    dehy_str = string_append(dehy_str, "\n======== timeout_queues =========");
	for (k = kh_begin(dehydrator->timeout_queues); k != kh_end(dehydrator->timeout_queues); ++k)
	{
        if (kh_exist(dehydrator->timeout_queues, k))
		{
            ElementList* list = kh_value(dehydrator->timeout_queues, k);
            dehy_str = string_append(dehy_str, "\n>>List: ");
            char qnum[50];
            sprintf(qnum,"%d ", kh_key(dehydrator->timeout_queues, k));
            dehy_str = string_append(dehy_str, qnum);

            char* list_str = printList(list);
            dehy_str = string_append(dehy_str, list_str);
            RedisModule_Free(list_str);
		}
    }
    dehy_str = string_append(dehy_str, "\n");

    dehy_str = string_append(dehy_str, "\n======== element_nodes issues =========\n");
    int found_problems = 0;
	for (k = kh_begin(dehydrator->element_nodes); k != kh_end(dehydrator->element_nodes); ++k)
	{
		if (kh_exist(dehydrator->element_nodes, k))
		{
			ElementListNode* node = kh_value(dehydrator->element_nodes, k);
            if (!RMUtil_StringEqualsC(node->element_id, kh_key(dehydrator->element_nodes, k)))
            {
                dehy_str = string_append(dehy_str, RedisModule_StringPtrLen(node->element_id, NULL));
                dehy_str = string_append(dehy_str, "is stored under id: ");
                dehy_str = string_append(dehy_str, kh_key(dehydrator->element_nodes, k));
                dehy_str = string_append(dehy_str, "\n");
                found_problems = 1;
            }
    	}
	}
    if (!found_problems)
    {
        dehy_str = string_append(dehy_str, "no issues were found.\n");
    }
    dehy_str = string_append(dehy_str, "================================\n");

    return dehy_str;
}


void deleteDehydrator(Dehydrator* dehydrator)
{
    khiter_t k;

    // clear and delete the timeout_queues dictionary
	for (k = kh_begin(dehydrator->timeout_queues); k != kh_end(dehydrator->timeout_queues); ++k)
	{
        if (kh_exist(dehydrator->timeout_queues, k))
		{
            ElementList* list = kh_value(dehydrator->timeout_queues, k);
			kh_del(16, dehydrator->timeout_queues, k);
			deleteList(list);
		}
    }
    kh_destroy(16, dehydrator->timeout_queues);

    // clear and delete the element_nodes dictionary
	for (k = kh_begin(dehydrator->element_nodes); k != kh_end(dehydrator->element_nodes); ++k)
	{
		if (kh_exist(dehydrator->element_nodes, k))
		{
			kh_del(32, dehydrator->element_nodes, k);
    	}
	}
    kh_destroy(32, dehydrator->element_nodes);

    // delete the dehydrator
    // RedisModule_FreeString(ctx, dehydrator->name); //TODO: make this work
    RedisModule_Free(dehydrator);
}


ElementListNode* _getNodeForID(Dehydrator* dehydrator, RedisModuleString* element_id)
{
        // now we know we have a dehydrator get element node from element_nodes
		ElementListNode* node = NULL;

		khiter_t k = kh_get(32, dehydrator->element_nodes, RedisModule_StringPtrLen(element_id, NULL));  // first have to get iterator
		if (k != kh_end(dehydrator->element_nodes)) // k will be equal to kh_end if key not present
		{
			node = kh_val(dehydrator->element_nodes, k);
        }
        return node;
}

void _removeNodeFromMapping(Dehydrator* dehydrator, ElementListNode* node)
{
    khiter_t k = kh_get(32, dehydrator->element_nodes, RedisModule_StringPtrLen(node->element_id, NULL));  // first have to get iterator
    if (k != kh_end(dehydrator->element_nodes)) // k will be equal to kh_end if key not present
    {
        kh_del(32, dehydrator->element_nodes, k);
    }
}

//##########################################################
//#
//#                     REDIS Type
//#
//#########################################################

void DehydratorTypeRdbSave(RedisModuleIO *rdb, void *value)
{
    Dehydrator *dehy = value;
	RedisModule_SaveString(rdb, dehy->name);
	RedisModule_SaveUnsigned(rdb, kh_size(dehy->timeout_queues));
	// for each timeout_queue in timeout_queues
	khiter_t k;
	for (k = kh_begin(dehy->timeout_queues); k != kh_end(dehy->timeout_queues); ++k)
	{
		if (!kh_exist(dehy->timeout_queues, k)) continue;
		ElementList* list = kh_value(dehy->timeout_queues, k);
		int ttl = kh_key(dehy->timeout_queues, k);
		RedisModule_SaveUnsigned(rdb, ttl);
		RedisModule_SaveUnsigned(rdb, list->len);
		ElementListNode* node = list->head;
		int done_with_queue = 0;
		while (!done_with_queue)
		{
			if ((node != NULL))
			{
				RedisModule_SaveUnsigned(rdb, node->expiration);
				RedisModule_SaveString(rdb, node->element_id);
				RedisModule_SaveString(rdb, node->element);
                node = node->next;
			}
			else
			{
				done_with_queue = 1;
			}
		}
	}
}

void *DehydratorTypeRdbLoad(RedisModuleIO *rdb, int encver)
{
    if (encver != 0) { return NULL; }
	khiter_t k;
	RedisModuleString* name = RedisModule_LoadString(rdb);
    Dehydrator *dehy = _createDehydrator(name);
	//create an ElementListNode
	uint64_t queue_num = RedisModule_LoadUnsigned(rdb);
	while(queue_num--)
	{
		ElementList* timeout_queue = _createNewList();
		uint64_t ttl = RedisModule_LoadUnsigned(rdb);

		uint64_t node_num = RedisModule_LoadUnsigned(rdb);
		while(node_num--)
		{
            uint64_t expiration = RedisModule_LoadUnsigned(rdb);
			RedisModuleString* element_id = RedisModule_LoadString(rdb);
            RedisModuleString* element = RedisModule_LoadString(rdb);

			ElementListNode* node  = _createNewNode(element, element_id, ttl, expiration);
			_listPush(timeout_queue, node);

			// mark element dehytion location in element_nodes
			int retval;
			k = kh_put(32, dehy->element_nodes, RedisModule_StringPtrLen(element_id, NULL), &retval);
			kh_value(dehy->element_nodes, k) = node;
		}

		int retval;
        k = kh_put(16, dehy->timeout_queues, ttl, &retval);
        kh_value(dehy->timeout_queues, k) = timeout_queue;
	}

    return dehy;
}

void DehydratorTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value)
{
	// REDISMODULE_NOT_USED(aof);
	// REDISMODULE_NOT_USED(key);
	// REDISMODULE_NOT_USED(value);
	// TODO: stub
	// struct DehydratorTypeObject *hto = value;
    // struct DehydratorTypeNode *node = hto->head;
    // while(node) {
    //     RedisModule_EmitAOF(aof,"HELLOTYPE.INSERT","sl",key,node->value);
    //     node = node->next;
    // }
}

void DehydratorTypeDigest(RedisModuleDigest *digest, void *value)
{
    // REDISMODULE_NOT_USED(digest);
    // REDISMODULE_NOT_USED(value);
    /* TODO: The DIGEST module interface is yet not implemented. */
}

void DehydratorTypeFree(void *value)
{
    deleteDehydrator(value);
}


//##########################################################
//#
//#                     REDIS Commands
//#
//#########################################################

int UpdateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 4)
    {
      return RedisModule_WrongArity(ctx);
    }

    RedisModuleString* dehydrator_name = argv[1];
    RedisModuleString* element_id = argv[2];
    RedisModuleString* updated_element = argv[3];

    // get key dehydrator_name
    RedisModuleKey *key = RedisModule_OpenKey(ctx, dehydrator_name,
        REDISMODULE_READ|REDISMODULE_WRITE);
	Dehydrator * dehydrator = validateDehydratorKey(ctx, key, NULL);
	if (dehydrator == NULL)
    {
        RedisModule_ReplyWithError(ctx, "ERROR: No Such dehydrator.");
        return REDISMODULE_ERR;
    }

    ElementListNode* node = _getNodeForID(dehydrator, element_id);
    if (node == NULL)
    {
        RedisModule_ReplyWithError(ctx, "ERROR: No Such Element.");
        return REDISMODULE_ERR;
    } // no element with such element_id

    //send reply to user
    RedisModule_ReplyWithString(ctx, node->element);
    RedisModule_Free(node->element);
    node->element = RedisModule_CreateStringFromString(ctx, updated_element);

    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}

int TimeToNextCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{

    if (argc != 2)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModuleString* dehydrator_name = argv[1];

    // get key dehydrator_name
    RedisModuleKey *key = RedisModule_OpenKey(ctx, dehydrator_name, REDISMODULE_READ);
    Dehydrator* dehydrator = validateDehydratorKey(ctx, key, NULL);
    if (dehydrator == NULL)
    {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }

    time_t now = current_time_ms();
    int time_to_next = -1;

	khiter_t k;
	for (k = kh_begin(dehydrator->timeout_queues); k != kh_end(dehydrator->timeout_queues); ++k)
	{
		if (!kh_exist(dehydrator->timeout_queues, k)) continue;
        ElementList* list = kh_value(dehydrator->timeout_queues, k);
        ElementListNode* head = list->head;
        if (head != NULL)
        {
            int tmp = head->expiration - now;
            if (tmp <= 0)
            {
                time_to_next = 0;
                break;
            }
            else if ((tmp < time_to_next) || (time_to_next < 0))
            {
                time_to_next = tmp;
            }
        }
    }

    RedisModule_CloseKey(key);
    RedisModule_ReplyWithLongLong(ctx, time_to_next);
    return REDISMODULE_OK;
}

int PrintCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 2)
    {
      return RedisModule_WrongArity(ctx);
    }

    // get key dehydrator_name
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    Dehydrator* dehydrator = validateDehydratorKey(ctx, key, NULL);
    if (dehydrator == NULL)
    {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }

    char* dehy_str = printDehydrator(dehydrator);
    RedisModule_CloseKey(key);
    RedisModule_ReplyWithSimpleString(ctx, dehy_str);
    RedisModule_Free(dehy_str);
    return REDISMODULE_OK;
}


int LookCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 3)
    {
      return RedisModule_WrongArity(ctx);
    }
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    Dehydrator * dehydrator = validateDehydratorKey(ctx, key, NULL);
    if (dehydrator == NULL)
    {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }

    ElementListNode* node = _getNodeForID(dehydrator, argv[2]);

    if ((node != NULL) && (node->element != NULL))
    {
        RedisModule_ReplyWithString(ctx, node->element);
        RedisModule_CloseKey(key);
        return REDISMODULE_OK;
    }

    RedisModule_ReplyWithNull(ctx);
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}



/*
* dehydrator.gidpush <timeout> <element>
* dehydrate <element> for <timeout> seconds
*/
int GIDPushCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 4)
    {
      return RedisModule_WrongArity(ctx);
    }

    RedisModuleString* dehydrator_name = argv[1];
    RedisModuleString * timeout = argv[2];
    RedisModuleString * element = argv[3];

    char* tmp = generate_id();
    RedisModuleString * element_id = RedisModule_CreateString(ctx, tmp, ID_LENGTH);
    RedisModule_Free(tmp);

    // timeout str to int ttl
    long long ttl;
    int rep = RedisModule_StringToLongLong(timeout, &ttl);
    if (rep == REDISMODULE_ERR) { return REDISMODULE_ERR; }

    // get key dehydrator_name
    RedisModuleKey *key = RedisModule_OpenKey(ctx, dehydrator_name,
        REDISMODULE_READ|REDISMODULE_WRITE);
    Dehydrator* dehydrator = validateDehydratorKey(ctx, key, dehydrator_name);
    if (dehydrator == NULL)
    {
        RedisModule_ReplyWithError(ctx, "ERROR: Not a dehydrator.");
        return REDISMODULE_ERR;
    }

    // now we know we have a dehydrator check if there is anything in id = element_id
    ElementListNode* node = _getNodeForID(dehydrator, element_id);
    if (node != NULL) // somthing is already there
    {
        RedisModule_ReplyWithError(ctx, "ERROR: Element already dehydrating.");
        return REDISMODULE_ERR;
    }

    // get timeout_queues[ttl]
    ElementList* timeout_queue = NULL;
    khiter_t k = kh_get(16, dehydrator->timeout_queues, ttl);  // first have to get iterator
    if (k != kh_end(dehydrator->timeout_queues)) // k will be equal to kh_end if key not present
    {
        timeout_queue = kh_val(dehydrator->timeout_queues, k);
    }
    if (timeout_queue == NULL) //does not exist
    {
        // create an empty ElementList and add it to timeout_queues
        timeout_queue = _createNewList();
        int retval;
        k = kh_put(16, dehydrator->timeout_queues, ttl, &retval);
        kh_value(dehydrator->timeout_queues, k) = timeout_queue;
    }

    //let's make our own copy of these
    RedisModuleString* saved_element_id = RedisModule_CreateStringFromString(ctx, element_id);
    RedisModuleString* saved_element = RedisModule_CreateStringFromString(ctx, element);

    //create an ElementListNode
    node  = _createNewNode(saved_element, saved_element_id, ttl, current_time_ms() + ttl);

    // push to tail of the list
    _listPush(timeout_queue, node);

    // mark element dehytion location in element_nodes
    int retval;
    k = kh_put(32, dehydrator->element_nodes, RedisModule_StringPtrLen(saved_element_id, NULL), &retval);
    kh_value(dehydrator->element_nodes, k) = node;

    RedisModule_ReplyWithString(ctx, element_id);
    RedisModule_FreeString(ctx,element_id);
    RedisModule_CloseKey(key);

    return REDISMODULE_OK;
}

/*
* dehydrator.push <timeout> <element> <element_id>
* dehydrate <element> for <timeout> seconds
*/
int PushCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if(argc != 5)
    {
      return RedisModule_WrongArity(ctx);
    }

    RedisModuleString* dehydrator_name = argv[1];
    RedisModuleString * timeout = argv[2];
    RedisModuleString * element = argv[3];
    RedisModuleString * element_id = argv[4];

    // timeout str to int ttl
    long long ttl;
    int rep = RedisModule_StringToLongLong(timeout, &ttl);
    if (rep == REDISMODULE_ERR) { return REDISMODULE_ERR; }

    // get key dehydrator_name
    RedisModuleKey *key = RedisModule_OpenKey(ctx, dehydrator_name,
        REDISMODULE_READ|REDISMODULE_WRITE);
    Dehydrator* dehydrator = validateDehydratorKey(ctx, key, dehydrator_name);
    if (dehydrator == NULL)
    {
        RedisModule_ReplyWithError(ctx, "ERROR: Not a dehydrator.");
        return REDISMODULE_ERR;
    }

    // now we know we have a dehydrator check if there is anything in id = element_id
    ElementListNode* node = _getNodeForID(dehydrator, element_id);
    if (node != NULL) // somthing is already there
    {
        RedisModule_ReplyWithError(ctx, "ERROR: Element already dehydrating.");
        return REDISMODULE_ERR;
    }

    // get timeout_queues[ttl]
	ElementList* timeout_queue = NULL;
	khiter_t k = kh_get(16, dehydrator->timeout_queues, ttl);  // first have to get iterator
	if (k != kh_end(dehydrator->timeout_queues)) // k will be equal to kh_end if key not present
	{
		timeout_queue = kh_val(dehydrator->timeout_queues, k);
	}
    if (timeout_queue == NULL) //does not exist
    {
        // create an empty ElementList and add it to timeout_queues
        timeout_queue = _createNewList();
		int retval;
        k = kh_put(16, dehydrator->timeout_queues, ttl, &retval);
        kh_value(dehydrator->timeout_queues, k) = timeout_queue;
    }

    //let's make our own copy of these
    RedisModuleString* saved_element_id = RedisModule_CreateStringFromString(ctx, element_id);
    RedisModuleString* saved_element = RedisModule_CreateStringFromString(ctx, element);

    //create an ElementListNode
    node  = _createNewNode(saved_element, saved_element_id, ttl, current_time_ms() + ttl);

    // push to tail of the list
    _listPush(timeout_queue, node);

    // mark element dehytion location in element_nodes
    int retval;
	k = kh_put(32, dehydrator->element_nodes, RedisModule_StringPtrLen(saved_element_id, NULL), &retval);
	kh_value(dehydrator->element_nodes, k) = node;
    if (argc == 4)
    {
        RedisModule_ReplyWithString(ctx, element_id);
        RedisModule_FreeString(ctx,element_id);
    }
    else
    {
        RedisModule_ReplyWithSimpleString(ctx, "OK");
    }
    RedisModule_CloseKey(key);

    return REDISMODULE_OK;
}


/*
* dehydrator.pull <element_id>
* Pull an element off the bench by id.
*/
int PullCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 3)
    {
      return RedisModule_WrongArity(ctx);
    }

    // get key dehydrator_name
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
        REDISMODULE_READ|REDISMODULE_WRITE);
    Dehydrator * dehydrator = validateDehydratorKey(ctx, key, NULL);
    if (dehydrator == NULL)
    {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }

    ElementListNode* node = _getNodeForID(dehydrator, argv[2]);
    if (node != NULL)
    {

        _listPull(dehydrator, node);
        _removeNodeFromMapping(dehydrator, node);

        if (node->element == NULL)
        {
            RedisModule_ReplyWithNull(ctx);
        }
        else
        {
            RedisModule_ReplyWithString(ctx, node->element);
        }
        deleteNode(node);
    }
    else
    {
        // no element with such element_id
        RedisModule_ReplyWithNull(ctx);
    }
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}

/*
* dehydrator.poll
* get all elements which were dried for long enogh
*/
int PollCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 2)
    {
      return RedisModule_WrongArity(ctx);
    }

    // get key for dehydrator
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
        REDISMODULE_READ|REDISMODULE_WRITE);
    Dehydrator* dehydrator = validateDehydratorKey(ctx, key, NULL);
    if (dehydrator == NULL)
    {
        RedisModule_ReplyWithArray(ctx, 0);
        return REDISMODULE_OK;
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    int expired_element_num = 0;
    time_t now = current_time_ms();
    // for each timeout_queue in timeout_queues
    khiter_t k;
    for (k = kh_begin(dehydrator->timeout_queues); k != kh_end(dehydrator->timeout_queues); ++k)
    {
        if (!kh_exist(dehydrator->timeout_queues, k)) continue;
        ElementList* list = kh_value(dehydrator->timeout_queues, k);
        int done_with_queue = 0;
        while ((list != NULL) && (!done_with_queue))
        {
            ElementListNode* head = list->head;
            if ((head != NULL) && (head->expiration <= now))
            {
                ElementListNode* node = _listPop(list);
                _removeNodeFromMapping(dehydrator, node);
                RedisModule_ReplyWithString(ctx, node->element); // append node->element to output
                deleteNode(node);
                ++expired_element_num;
            }
            else
            {
                if (list->len == 0)
                {
                    deleteList(list);
                    kh_del(16, dehydrator->timeout_queues, k);
                }
                done_with_queue = 1;
            }
        }
    }
    RedisModule_ReplySetArrayLength(ctx, expired_element_num);
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}


int TestLook(RedisModuleCtx *ctx)
{
    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_look");
    printf("Testing Look - ");

    // size_t len
    RedisModuleCallReply *check1 =
        RedisModule_Call(ctx, "REDE.look", "cc", "TEST_DEHYDRATOR_look", "test_element");
    RMUtil_Assert(!RedisModule_CreateStringFromCallReply(check1));
    // check if X is dehydtaring (should be false)
    // RMUtil_Assert(RedisModule_CreateStringFromCallReply(check1) == NULL);


    RedisModuleCallReply *push1 =
        RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_look", "100000", "payload", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check2 =
        RedisModule_Call(ctx, "REDE.look", "cc", "TEST_DEHYDRATOR_look", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(check2) != REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(RedisModule_CreateStringFromCallReply(check2) != NULL); //TODO: == "payload"


    RedisModuleCallReply *pull1 =
        RedisModule_Call(ctx, "REDE.pull", "cc", "TEST_DEHYDRATOR_look", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(pull1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check3 =
        RedisModule_Call(ctx, "REDE.look", "cc", "TEST_DEHYDRATOR_look", "test_element");
    RMUtil_Assert(!RedisModule_CreateStringFromCallReply(check3));

    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_look");
    printf("Passed.\n");
    return REDISMODULE_OK;
}


int TestUpdate(RedisModuleCtx *ctx)
{
    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_update");
    printf("Testing Update - ");

    RedisModuleCallReply *check1 =
        RedisModule_Call(ctx, "REDE.update", "ccc", "TEST_DEHYDRATOR_update", "test_element", "text");
    RMUtil_Assert(RedisModule_CallReplyType(check1) == REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *push1 =
        RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_update", "100000", "some payload", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check2 =
        RedisModule_Call(ctx, "REDE.update", "ccc", "TEST_DEHYDRATOR_update", "test_element", "some OTHER payload");
    RMUtil_Assert(RedisModule_CallReplyType(check2) != REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(RMUtil_StringEqualsC(RedisModule_CreateStringFromCallReply(check2),"some payload"));

    RedisModuleCallReply *check3 =
        RedisModule_Call(ctx, "REDE.look", "cc", "TEST_DEHYDRATOR_update", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(check3) != REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(RMUtil_StringEqualsC(RedisModule_CreateStringFromCallReply(check3), "some OTHER payload"));

    RedisModuleCallReply *pull1 =
        RedisModule_Call(ctx, "REDE.pull", "cc", "TEST_DEHYDRATOR_update", "test_element");
    RMUtil_Assert(RedisModule_CallReplyType(pull1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check4 =
        RedisModule_Call(ctx, "REDE.update", "ccc", "TEST_DEHYDRATOR_update", "test_element", "text");
    RMUtil_Assert(RedisModule_CallReplyType(check4) == REDISMODULE_REPLY_ERROR);

    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_update");
    printf("Passed.\n");
    return REDISMODULE_OK;
}


int TestTimeToNext(RedisModuleCtx *ctx)
{
    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_ttn");
    printf("Testing TTN - ");

    RedisModuleCallReply *push1 =
        RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_ttn", "3000", "payload", "ttn_test_element");
    RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check1 =
        RedisModule_Call(ctx, "REDE.ttn", "c", "TEST_DEHYDRATOR_ttn");
    RMUtil_Assert(RedisModule_CallReplyType(check1) != REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(RedisModule_CallReplyInteger(check1) == 3000);

    sleep(2);

    RedisModuleCallReply *check2 =
        RedisModule_Call(ctx, "REDE.ttn", "c", "TEST_DEHYDRATOR_ttn");
    RMUtil_Assert(RedisModule_CallReplyType(check2) != REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(RedisModule_CallReplyInteger(check2) == 1000);

    sleep(2);

    RedisModuleCallReply *check3 =
        RedisModule_Call(ctx, "REDE.ttn", "c", "TEST_DEHYDRATOR_ttn");
    RMUtil_Assert(RedisModule_CallReplyType(check3) != REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(RedisModule_CallReplyInteger(check3) == 0);


    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_ttn");
    printf("Passed.\n");
    return REDISMODULE_OK;
}


int TestPush(RedisModuleCtx *ctx)
{
    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_push");
    printf("Testing Push - ");
    // char * element_id = "push_test_element";
    // size_t len
    RedisModuleCallReply *check1 =
        RedisModule_Call(ctx, "REDE.look", "cc", "TEST_DEHYDRATOR_push", "push_test_element");
    RMUtil_Assert(!RedisModule_CreateStringFromCallReply(check1));
    // check if X is dehydtaring (should be false)
    // RMUtil_Assert(RedisModule_CreateStringFromCallReply(check1) == NULL);

    RedisModuleCallReply *push1 =
        RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_push", "1000", "payload", "push_test_element");
    RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);

    RedisModuleCallReply *check2 =
        RedisModule_Call(ctx, "REDE.look", "cc", "TEST_DEHYDRATOR_push", "push_test_element");
    RMUtil_Assert(RedisModule_CallReplyType(check2) != REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(RedisModule_CreateStringFromCallReply(check2) != NULL); //TODO: == "payload"

    // TODO: test pushing more then one element
    // TODO: add fail-case tests

    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_push");
    printf("Passed.\n");
    return REDISMODULE_OK;
}


int TestPull(RedisModuleCtx *ctx)
{
    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_pull");
    printf("Testing Pull - ");

    char * store_key = "pull_test_element";
    char * bad_store_key = "pull_test_bad_element";

    RedisModuleCallReply *push1 =
        RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_pull", "100000", "payload", store_key);
    RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);


    RedisModuleCallReply *look1 =
        RedisModule_Call(ctx, "REDE.look", "cc", "TEST_DEHYDRATOR_pull", store_key);
    // RMUTIL_ASSERT_NOERROR(look1);
    RMUtil_Assert(RedisModule_CallReplyType(look1) != REDISMODULE_REPLY_ERROR);
    // RMUtil_Assert(RedisModule_CallReplyInteger(rep1) == 1);


    RedisModuleCallReply *pull1 =
        RedisModule_Call(ctx, "REDE.pull", "cc", "TEST_DEHYDRATOR_pull", bad_store_key);
    RMUtil_Assert(!RedisModule_CreateStringFromCallReply(pull1));


    RedisModuleCallReply *look2 =
        RedisModule_Call(ctx, "REDE.look", "cc", "TEST_DEHYDRATOR_pull", bad_store_key);
    RMUtil_Assert(!RedisModule_CreateStringFromCallReply(look2));
    // RMUtil_Assert(RedisModule_CallReplyInteger(rep2) == 0);


    RedisModuleCallReply *pull2 =
        RedisModule_Call(ctx, "REDE.pull", "cc", "TEST_DEHYDRATOR_pull", store_key);
    RMUtil_Assert(RedisModule_CallReplyType(pull2) != REDISMODULE_REPLY_ERROR);


    RedisModuleCallReply *look3 =
        RedisModule_Call(ctx, "REDE.look", "cc", "TEST_DEHYDRATOR_pull", bad_store_key);
    RMUtil_Assert(!RedisModule_CreateStringFromCallReply(look3));

    printf("Passed.\n");
    RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_pull");;
    return REDISMODULE_OK;
}


int TestPoll(RedisModuleCtx *ctx)
{
    printf("Testing Poll - ");

  // clear dehydrator
  RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_poll");

  // start test
  // push elements 1, 4, 7 & 3a (for 1, 4, 7 & 3 seconds)
  // 1
  RedisModuleCallReply *push1 =
      RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_poll", "1000", "element_1", "e1");
  RMUtil_Assert(RedisModule_CallReplyType(push1) != REDISMODULE_REPLY_ERROR);

  // 4
  RedisModuleCallReply *push4 =
      RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_poll", "4000", "element_4", "e4");
  RMUtil_Assert(RedisModule_CallReplyType(push4) != REDISMODULE_REPLY_ERROR);

  // 7
  RedisModuleCallReply *push7 =
      RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_poll", "7000", "element_7", "e7");
  RMUtil_Assert(RedisModule_CallReplyType(push7) != REDISMODULE_REPLY_ERROR);

  // 3a
  RedisModuleCallReply *push3a =
      RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_poll", "3000", "element_3a", "e3a");
  RMUtil_Assert(RedisModule_CallReplyType(push3a) != REDISMODULE_REPLY_ERROR);

  // pull question 7
  RedisModuleCallReply *pull_seven_rep =
      RedisModule_Call(ctx, "REDE.pull", "cc", "TEST_DEHYDRATOR_poll", "e7");
  RMUtil_Assert(RedisModule_CallReplyType(pull_seven_rep) != REDISMODULE_REPLY_ERROR);

  // poll - make sure no element pops right out
  RedisModuleCallReply *poll1_rep =
      RedisModule_Call(ctx, "REDE.poll", "c", "TEST_DEHYDRATOR_poll");
  RMUtil_Assert(RedisModule_CallReplyType(poll1_rep) != REDISMODULE_REPLY_ERROR);
  RMUtil_Assert(RedisModule_CallReplyLength(poll1_rep) == 0);

  // sleep 1 sec
  sleep(1);
  // push element 3b (for 3 seconds)
  // 3b
  RedisModuleCallReply *push_three_b =
      RedisModule_Call(ctx, "REDE.push", "cccc", "TEST_DEHYDRATOR_poll", "3000", "element_3b", "e3b");
  RMUtil_Assert(RedisModule_CallReplyType(push_three_b) != REDISMODULE_REPLY_ERROR);

  // poll (t=1) - we expect only element 1 to pop out
  RedisModuleCallReply *poll_two_rep =
      RedisModule_Call(ctx, "REDE.poll", "c", "TEST_DEHYDRATOR_poll");
  RMUtil_Assert(RedisModule_CallReplyType(poll_two_rep) != REDISMODULE_REPLY_ERROR);
  RMUtil_Assert(RedisModule_CallReplyLength(poll_two_rep) == 1);
  RedisModuleCallReply *subreply_a = RedisModule_CallReplyArrayElement(poll_two_rep, 0);
  RMUtil_AssertReplyEquals(subreply_a, "element_1")

  // sleep 2 secs and poll (t=3) - we expect only element 3a to pop out
  sleep(2);
  RedisModuleCallReply *poll_three_rep =
      RedisModule_Call(ctx, "REDE.poll", "c", "TEST_DEHYDRATOR_poll");
  RMUtil_Assert(RedisModule_CallReplyType(poll_three_rep) != REDISMODULE_REPLY_ERROR);
  RMUtil_Assert(RedisModule_CallReplyLength(poll_three_rep) == 1);
  RedisModuleCallReply *subreply_b = RedisModule_CallReplyArrayElement(poll_three_rep, 0);
  RMUtil_AssertReplyEquals(subreply_b, "element_3a");


  // sleep 2 secs and poll (t=5) - we expect elements 4 and 3b to pop out
  sleep(2);
  RedisModuleCallReply *poll_four_rep =
      RedisModule_Call(ctx, "REDE.poll", "c", "TEST_DEHYDRATOR_poll");
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
  RedisModuleCallReply *poll_five_rep = RedisModule_Call(ctx, "REDE.poll", "c", "TEST_DEHYDRATOR_poll");
  RMUtil_Assert(RedisModule_CallReplyType(poll_five_rep) != REDISMODULE_REPLY_ERROR);
  RMUtil_Assert(RedisModule_CallReplyLength(poll_five_rep) == 0);

  // clear dehydrator
  RedisModule_Call(ctx, "DEL", "c", "TEST_DEHYDRATOR_poll");
  printf("Passed.\n");
  return REDISMODULE_OK;
}


// Unit test entry point for the module
int TestModule(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{

    RMUtil_Test(TestLook);
    RMUtil_Test(TestPush);
    RMUtil_Test(TestPull);
    RMUtil_Test(TestPoll);
    RMUtil_Test(TestTimeToNext);
    RMUtil_Test(TestUpdate);
    printf("All Tests Passed Succesfully!\n");

    RedisModule_ReplyWithSimpleString(ctx, "PASS");
    return REDISMODULE_OK;
}


int RedisModule_OnLoad(RedisModuleCtx *ctx)
{
    // Register the module itself
    if (RedisModule_Init(ctx, "REDE", 1, REDISMODULE_APIVER_1) ==
      REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    DehydratorType = RedisModule_CreateDataType(ctx, "dehy-type", 0,
        DehydratorTypeRdbLoad,
        DehydratorTypeRdbSave,
        DehydratorTypeAofRewrite,
        DehydratorTypeDigest,
        DehydratorTypeFree
    );
    if (DehydratorType == NULL) return REDISMODULE_ERR;

    // register TimeToNextCommand - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "REDE.TTN", TimeToNextCommand);

    // register dehydrator.update - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "REDE.UPDATE", UpdateCommand);

    // register dehydrator.push - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "REDE.PUSH", PushCommand);

    // register dehydrator.pull - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "REDE.PULL", PullCommand);

    // register dehydrator.poll - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "REDE.POLL", PollCommand);

    // register dehydrator.look - using the shortened utility registration macro
    RMUtil_RegisterReadCmd(ctx, "REDE.LOOK", LookCommand);

    // register dehydrator.gidpush - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "REDE.GIDPUSH", GIDPushCommand);

    //  TEST OUTPUTS TO THE SERVER SIDE, USE WITH CAUTION
    // register the unit test
    RMUtil_RegisterWriteCmd(ctx, "REDE.TEST", TestModule);

    // PRINT command works OK, but it is meant for debuging perposes, so it will not be listed in the documentation
    // register dehydrator.print - using the shortened utility registration macro
    RMUtil_RegisterWriteCmd(ctx, "REDE.PRINT", PrintCommand);



    return REDISMODULE_OK;
}
