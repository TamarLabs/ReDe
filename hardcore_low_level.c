/*****************************************************
 *
 * this is a c implementation of the dehydration module
 *
******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "rmutil/util.h"
#include "khash.h"
#include "hardcore_low_level.h"
#include "rmutil/alloc.h"

//##########################################################
//#
//#                     Linked List
//#
//#########################################################

//Creates a new Node and returns pointer to it.
ElementListNode* _createNewNode(void* element, const char* element_id, long long expiration)
{
	ElementListNode* newNode
		= (ElementListNode*)RedisModule_Alloc(sizeof(ElementListNode));

    newNode->element_id = RedisModule_Strdup(element_id);
    newNode->element = element;
	newNode->expiration = expiration;
	newNode->next = NULL;
    newNode->prev = NULL;
	return newNode;
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
   ElementListNode* head = list->head;

   if (head == NULL) { return NULL; } // if list empty
   if (head->next == NULL) { list->tail = NULL; } // if only one link

   // swap to new head
   list->head = head->next;
   list->len = list->len - 1;
   list->head->prev = NULL;

   return head;
}


void _listPull(ElementListNode* node)
{
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

    if (current == list->tail) {
        list->tail = node->prev;
        list->tail->next = NULL;
    }
    else
    {
        node->next->prev = node->prev;
    }
}

// pull from list and return an element with the following id
ElementListNode* _listFind(ElementList* list, const char* element_id)
{
    //start from head
    ElementListNode* current = list->head;
    ElementListNode* previous = NULL;

    if (current == NULL) { return NULL; } //list is empty

    // iterate over queue and find the element that has id = element_id
    while (current->element_id != element_id)
    {
        if (current->next == NULL) { return NULL; } // got to tail
        else
        {
            previous = current; //store reference to current node
            current = current->next; //move to next node
        }
    }

    while (current->element_id == element_id) // match found
    {
        return current;
    }

    return NULL;
}


void deleteNode(ElementListNode* node)
{
    // free everything else related to the node
    RedisModule_Free(node->element_id);
    RedisModule_Free(node->element);
    RedisModule_Free(node);
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


//##########################################################
//#
//#                    Dictionary
//#
//#########################################################

ForwardIndexEntry *ForwardIndexIterator_Next(ForwardIndexIterator *iter) {
    // advance the iterator while it's empty
    while (iter->k != kh_end(iter->idx->hits) && !kh_exist(iter->idx->hits, iter->k)) {
        ++iter->k;
    }

    // if we haven't reached the end, return the current iterator's entry
    if (iter->k != kh_end(iter->idx->hits) && kh_exist(iter->idx->hits, iter->k)) {
        ForwardIndexEntry *entry = kh_value(iter->idx->hits, iter->k);
        ++iter->k;
        return entry;
    }

    return NULL;
}

//##########################################################
//#
//#                     Utilities
//#
//#########################################################

Dehydrator* _createDehydrator(char* dehydrator_name)
{

    Dehydrator* dehy
		= (dehydrator*)RedisModule_Alloc(sizeof(dehydrator));

    dehy->timeout_queues = kh_init(16);
    dehy->element_nodes = kh_init(32);
	dehy->name = dehydrator_name;

    return dehy;
}


Dehydrator* getDehydrator(char* dehydrator_name)
{
    // // get key dehydrator_name
    // // TODO:
    // if (dehydrator_name does not EXIST)
    // {
    //    Dehydrator* dehydrator = _createDehydrator(dehydrator_name);
    //    //save into redis under dehydrator_name
    //    return dehydrator;
    // }
    // else if (key contains somthing that is not a dehydrator)
    // {
    //     return NULL;
    // }
    // return content of the key;
    return NULL;
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
    RedisModule_Free(dehydrator);
}


void* pull(Dehydrator* dehydrator, char* element_id, ElementList* timeout_queue)
{
	ElementListNode* node = NULL;
	khiter_t k = kh_get(32, dehydrator->element_nodes, element_id);  // first have to get iterator
	if (k != kh_end(dehydrator->element_nodes)) // k will be equal to kh_end if key not present
	{
		node = kh_val(dehydrator->element_nodes, k);
	}

	if (node == NULL) { return NULL; } // no such element in the system

    void* element = node->element; // extract element

    // delete element_nodes[element_id]
    kh_del(32, dehydrator->element_nodes, k);

    // free everything else related to the node but not the element
    RedisModule_Free(node->element_id);
    RedisModule_Free(node);

    return element;
}

ElementListNode* _getNodeForID(char* dehydrator_name, char* element_id)
{
        // get key dehydrator_name
        Dehydrator* dehydrator = getDehydrator(dehydrator_name);
        if (dehydrator == NULL)
        {
            //TODO: set error to be no such dehydrator
            return NULL;
        }

        // now we know we have a dehydrator get element node from element_nodes
		ElementListNode* node = NULL;
		khiter_t k = kh_get(32, dehydrator->element_nodes, element_id);  // first have to get iterator
		if (k != kh_end(dehydrator->element_nodes)) // k will be equal to kh_end if key not present
		{
			node = kh_val(dehydrator->element_nodes, k);
		}
        return node;
}

char* _toQueueName(int ttl)
{
	if (ttl > 315400000000) { ttl = 315400000000; } // you know, a decade has only 3.154e+11 milliseconds in it..
	char* queue_name = (char*)RedisModule_Alloc(sizeof(char)*32); // 12 + 14 + some good measure
	sprintf(queue_name, "timeout_queue#%s", ttl);
    return queue_name;
}


//##########################################################
//#
//#                     REDIS Type
//#
//#########################################################
//
// void DehydratorTypeRdbSave(RedisModuleIO *rdb, void *value)
// {
//     Dehydrator *dehy = value;
// 	RedisModule_SaveStringBuffer(rdb, dehy->name);
// 	int queue_num = kh_size(dehy->timeout_queues);
//     RedisModule_SaveUnsigned(rdb, queue_num);
// 	while(queue_num--)
// 	{
// 		_listPop
// 	}
//
//     while(node) { // TODO: all of this should be replaced
//         RedisModule_SaveSigned(rdb,node->value);
//         node = node->next;
//     }
// }
//
//
// void *DehydratorTypeRdbLoad(RedisModuleIO *rdb, int encver) {
//     if (encver != 0) {
//         /* RedisModule_Log("warning","Can't load data with version %d", encver);*/
//         return NULL;
//     }
// 	int lenptr;
// 	char* name = RedisModule_LoadStringBuffer(rdb, &lenptr);
//     Dehydrator *dehy = _createDehydrator(name);
//     while(elements--) {
//         int64_t ele = RedisModule_LoadSigned(rdb);
//         DehydratorTypeInsert(dehy,ele);
//     }
//     return dehy;
// }

//
// void DehydratorTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
//     struct DehydratorTypeObject *hto = value;
//     struct DehydratorTypeNode *node = hto->head;
//     while(node) {
//         RedisModule_EmitAOF(aof,"HELLOTYPE.INSERT","sl",key,node->value);
//         node = node->next;
//     }
// }
//
// void DehydratorTypeDigest(RedisModuleDigest *digest, void *value) {
//     REDISMODULE_NOT_USED(digest);
//     REDISMODULE_NOT_USED(value);
//     /* TODO: The DIGEST module interface is yet not implemented. */
// }

void DehydratorTypeFree(void *value)
{
    deleteDehydrator(value);
}


//##########################################################
//#
//#                     REDIS Commands
//#
//#########################################################

int PushCommand_impl(char* dehydrator_name, const char* element_id, void* element, int ttl)
{
    // get key dehydrator_name
    Dehydrator* dehydrator = getDehydrator(dehydrator_name);
    if (dehydrator == NULL) { return REDIS_ERR; } // no such dehydrator

    // now we know we have a dehydrator check if there is anything in id = element_id
    ElementListNode* node = _getNodeForID(dehydrator_name, element_id);
    if (node != NULL) // somthing is already there
    {
        return REDIS_ERR;
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

    //create an ElementListNode
    node  = _createNewNode(element, element_id, now() + ttl);

    // push to tail of the list
    _listPush(timeout_queue, node);

    // mark element dehytion location in element_nodes
	int retval;
	k = kh_put(32, dehydrator->element_nodes, element_id, &retval);
	kh_value(dehydrator->element_nodes, k) = node;

    return REDIS_OK;
}


void* PullCommand_impl(char* dehydrator_name, char* element_id)
{
    ElementListNode* node = _getNodeForID(dehydrator_name, element_id)
    if (node == NULL) { return REDIS_ERR; } // no element with such element_id

    _listPull(node);

    return node->element;
}


ElementList* PollCommand_impl(char* dehydrator_name)
{
    // get key dehydrator_name
    Dehydrator* dehydrator = getDehydrator(dehydrator_name);
    if (dehydrator == NULL) { return REDIS_ERR; } // no such dehydrator

    ElementList* pulled_elements = _createNewList(); // TODO: use vector

    // for each timeout_queue in timeout_queues
	khiter_t k;
	for (k = kh_begin(dehydrator->timeout_queues); k != kh_end(dehydrator->timeout_queues); ++k)
	{
        if (!kh_exist(dehydrator->timeout_queues, k)) continue;
        ElementList* list = kh_value(dehydrator->timeout_queues, k);
        boolean done_with_queue = false;
        while (!done_with_queue)
        {
            ElementListNode* head = list->head;
            if ((head != NULL) && (head->expiration < now()))
            {
                _listPush(pulled_elements ,_listPop(list)); // append head->element to output
            }
            else
            {
                if (timeout_queue->len == 0)
                {
                    deleteList(list);
					kh_del(16, dehydrator->timeout_queues, k);
                }
                done_with_queue = true;
            }
        }
    }

    return pulled_elements;
}


void* LookCommand_impl(char* dehydrator_name, char* element_id)
{
    ElementListNode* node = _getNodeForID(dehydrator_name, element_id)
    if (node == NULL) { return REDIS_ERR; } // no element with such element_id

    return node->element;
}


int DeleteCommand_impl(char* dehydrator_name)
{
    Dehydrator* dehydrator = getDehydrator(dehydrator_name);
    if (dehydrator == NULL) { return REDIS_ERR; } // no such dehydrator

    deleteDehydrator(dehydrator);

    return REDIS_OK;
}


int UpdateCommand_impl(char* dehydrator_name, char* element_id,  void* updated_element)
{
    ElementListNode* node = _getNodeForID(dehydrator_name, element_id)
    if (node == NULL) { return REDIS_ERR; } // no element with such element_id

    node->element = updated_element;

    return REDIS_OK;
}


int TimeToNextCommand_impl(char* dehydrator_name)
{
    // get key dehydrator_name
    Dehydrator* dehydrator = getDehydrator(dehydrator_name);
    if (dehydrator == NULL) { return REDIS_ERR; } // no such dehydrator

    int time_to_next = -1;

	khiter_t k;
	for (k = kh_begin(dehydrator->timeout_queues); k != kh_end(dehydrator->timeout_queues); ++k)
	{
		if (!kh_exist(dehydrator->timeout_queues, k)) continue;
        ElementList* list = kh_value(dehydrator->timeout_queues, k);
        while (!done_with_queue)
        {
            ElementListNode* head = list->head
            if (head != NULL)
            {
                int tmp = head->expiration - now();
                if (tmp) < 0)
                {
                    return 0;
                }
                else if ((tmp < time_to_next) || (time_to_next < 0))
                {
                    time_to_next = tmp;
                }
            }
        }
    }

    return time_to_next;
}
