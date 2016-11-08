/*****************************************************
 *
 * this is a c implementation of the dehydration module
 *
******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dict.h"


//##########################################################
//#
//#                     Linked List
//#
//#########################################################

typedef struct element_list_node{
    void* element;
    char* element_id;
    long long expiration;
    element_list_node* next;
    element_list_node* prev;
} element_list_node;

typedef struct element_list{
    element_list_node* head;
    element_list_node* tail;
    int len;

} element_list;


//Creates a new Node and returns pointer to it.
element_list_node* _createNewNode(void* element, const char* element_id, long long expiration)
{
	element_list_node* newNode
		= (element_list_node*)malloc(sizeof(element_list_node));

    newNode->element_id = strdup(element_id);
    newNode->element = element;
	newNode->expiration = expiration;
	newNode->next = NULL;
    newNode->prev = NULL;
	return newNode;
}

//Creates a new Node and returns pointer to it.
element_list* _createNewList(void)
{
	element_list* list
		= (element_list*)malloc(sizeof(element_list));
	list->head = NULL;
	list->tail = NULL;
    list->len = 0;
	return list;
}


// insert a Node at tail of linked list
void _listPush(element_list* list, element_list_node* node)
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
element_list_node* _listPop(element_list* list) {
   element_list_node* head = list->head;

   if (head == NULL) { return NULL; } // if list empty
   if (head->next == NULL) { list->tail = NULL; } // if only one link

   // swap to new head
   list->head = head->next;
   list->len = list->len - 1;
   list->head->prev = NULL;

   return head;
}


void _listPull(element_list_node* node)
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
element_list_node* _listFind(element_list* list, const char* element_id)
{
    //start from head
    element_list_node* current = list->head;
    element_list_node* previous = NULL;

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


void deleteNode(element_list_node* node)
{
    // free everything else related to the node
    free(node->element_id);
    free(node->element);
    free(node);
}


void deleteList(element_list* list)
{
    element_list_node* current = list->head;

    // iterate over queue and find the element that has id = element_id
    while(current != NULL)
    {
        element_list_node* next = current->next; // save next
        deleteNode(current);
        current = next;  //move to next node
    }

    free(list);
}


//##########################################################
//#
//#                    Dictionary
//#
//#########################################################

//using implementation from redis'  "dict.h"

unsigned int dictCStringKeyHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, strlen((char*)key));
}

int dictCStringKeyCompare(void *privdata, const void *key1, const void *key2) {
    DICT_NOTUSED(privdata);
    return strcmp(key1,key2) == 0;
}

dictType dictType = {
    dictCStringKeyHash,        /* hash function */
    NULL,                      /* key dup */
    NULL,                      /* val dup */
    dictCStringKeyCompare,     /* key compare */
    NULL,                      /* key destructor */
    NULL                       /* val destructor */
};


//##########################################################
//#
//#                     Utilities
//#
//#########################################################

typedef struct dehydrator{
    dict *timeout_queues; //<ttl,element_list>
    dict *element_nodes; //<element_id,node*>
} Dehydrator;


Dehydrator* _createDehydrator(char* dehydrator_name)
{

    Dehydrator* dehy
		= (dehydrator*)malloc(sizeof(dehydrator));

    dehy->timeout_queues = dictCreate(&dictType,NULL);
    dehy->element_nodes = dictCreate(&dictType,NULL);

    return dehy;
}


Dehydrator* getDehydrator(char* dehydrator_name)
{
    // get key dehydrator_name
    // if dehydrator_name does not EXIST
    //    _createDehydrator(dehydrator_name)
    //    save into redis under dehydrator_name
    //    return that Dehydrator
    // else if key contains somthing that is not a dehydrator
    //    return NULL;
    // else if element_nodes already contains an element with id = element_id
    //    return NULL;
    // else
    // return content of the key
}


void deleteDehydrator(Dehydrator* dehydrator)
{
    dictEntry *de;

    // clear and delete the timeout_queues dictionary
    dictIterator *di = dictGetSafeIterator(dehydrator->timeout_queues);
    while ((de = dictNext(di)) != NULL)
    {
        element_list* list = dictGetVal(de);
        deleteList(list);
        dictDelete(dehydrator->timeout_queues, dictGetKey(de))

    }
    dictReleaseIterator(di);
    dictRelease(dehydrator->timeout_queues);

    // clear and delete the element_nodes dictionary
    di = dictGetSafeIterator(dehydrator->element_nodes);
    while ((de = dictNext(di)) != NULL)
    {
        dictDelete(dehydrator->element_nodes, dictGetKey(de))
    }
    dictReleaseIterator(di);
    dictRelease(dehydrator->element_nodes);

    // delete the dehydrator
    free(dehydrator);
}


void* pull(Dehydrator* dehydrator, char* element_id, element_list timeout_queue)
{
    element_list_node* node = dictFetchValue(dehydrator->element_nodes, element_id);
    if (node == NULL) { return NULL; } // no such element in the system

    void* element = node->element; // extract element

    // delete element_nodes[element_id] to NULL
    dictDelete(dehydrator->element_nodes, element_id)

    // free everything else related to the node but not the element
    free(node->element_id);
    free(node);

    return element;
}

element_list_node* _getNodeForID(char* dehydrator_name, char* element_id)
{
        // get key dehydrator_name
        Dehydrator* dehydrator = getDehydrator(dehydrator_name);
        if (dehydrator == NULL)
        {
            //TODO: set error to be no such dehydrator
            return NULL;
        }

        // now we know we have a dehydrator get element ttl from element_nodes
        return dictFetchValue(dehydrator->element_nodes, element_id);
}


//##########################################################
//#
//#                     REDIS COMMANDS
//#
//#########################################################

int PushCommand_impl(char* dehydrator_name, const char* element_id, void* element, long long ttl)
{
    // get key dehydrator_name
    Dehydrator* dehydrator = getDehydrator(dehydrator_name);
    if (dehydrator == NULL) { return REDIS_ERR; } // no such dehydrator

    // now we know we have a dehydrator check if there is anything in id = element_id
    element_list_node* node = dictFetchValue(dehydrator->element_nodes, element_id);
    if (node != NULL) // somthing is already there
    {
        return REDIS_ERR;
    }

    // get timeout_queues[ttl]
    element_list* timeout_queue = dictFetchValue(dehydrator->timeout_queues, ttl);
    if (timeout_queue == NULL) //does not exist
    {
        // create an empty element_list and add it to timeout_queues
        timeout_queue = _createNewList();
        dictAdd(dehydrator->timeout_queues, ttl, timeout_queue);
    }

    //create an element_list_node
    node  = _createNewNode(element, element_id, now() + ttl);

    // push to tail of the list
    _listPush(timeout_queue, node);

    // mark element dehytion location in element_nodes
    dictAdd(dehydrator->element_nodes, element_id, node);
    return REDIS_OK;
}


void* PullCommand_impl(char* dehydrator_name, char* element_id)
{
    element_list_node* node = _getNodeForID(char* dehydrator_name, char* element_id)
    if (node == NULL) { return REDIS_ERR; } // no element with such element_id

    _listPull(node);

    return node->element;
}


element_list* PollCommand_impl(char* dehydrator_name)
{
    // get key dehydrator_name
    Dehydrator* dehydrator = getDehydrator(dehydrator_name);
    if (dehydrator == NULL) { return REDIS_ERR; } // no such dehydrator

    element_list* pulled_elements = _createNewList();

    // for each timeout_queue in timeout_queues
    dictIterator *di = dictGetSafeIterator(dehydrator->timeout_queues);
    while ((de = dictNext(di)) != NULL)
    {
        boolean done_with_queue = false;
        element_list* list = dictGetVal(de);
        while (!done_with_queue)
        {
            element_list_node* head = timeout_queue->head
            if ((head != NULL) && (head->expiration < now()))
            {
                _listPush(pulled_elements ,_listPop(list)); // append head->element to output
            }
            else
            {
                if (timeout_queue->len == 0)
                {
                    deleteList(timeout_queues);
                    dictDelete(dehydrator->timeout_queues, dictGetKey(de))
                }
                done_with_queue = true;
            }
        }
    }
    dictReleaseIterator(di);

    return pulled_elements;
}


void* LookCommand_impl(char* dehydrator_name, char* element_id)
{
    element_list_node* node = _getNodeForID(char* dehydrator_name, char* element_id)
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
    element_list_node* node = _getNodeForID(char* dehydrator_name, char* element_id)
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

    dictIterator *di = dictGetSafeIterator(dehydrator->timeout_queues);
    while ((de = dictNext(di)) != NULL)
    {
        element_list* list = dictGetVal(de);
        while (!done_with_queue)
        {
            element_list_node* head = timeout_queue->head
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
    dictReleaseIterator(di);

    return time_to_next;
}
