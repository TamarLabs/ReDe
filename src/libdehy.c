#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include "libdehy.h"
// #include "utils.h"
#include "millisecond_time.h"
#include "trie/triemap.h"


// /* for redis module targets */
// #ifdef REDIS_MODULE_TARGET /* Set this when compiling your code as a module */
// #define MALLOC RedisModule_Alloc
// #define FREE RedisModule_Free
// #endif
/* for non redis module targets */
// #ifndef REDIS_MODULE_TARGET
#define rm_malloc  malloc
#define rm_free free
// #endif





char* printttNode(ElementListNode* node)
{
    char* node_str = (char*)malloc((node->element_len+256+256)*sizeof(char));
    // sprintf(node_str, "[elem=%s,ttl=%llu,exp=%llu]", node->element, node->ttl_queue, node->expiration);
    printf("[elem=%s,ttl=%llu,exp=%llu]\n", node->element, node->ttl_queue, node->expiration);
    return NULL;// return node_str;

}



/***************************
 *       Queue Utilities
 ***************************/

//Creates a new Node and returns pointer to it.
ElementListNode* NewNode(char* element, size_t element_len, mstime_t ttl)
{
    ElementListNode* newNode
        = (ElementListNode*)rm_malloc(sizeof(ElementListNode));
    newNode->element = strndup(element, element_len);
    newNode->element_len = element_len;
    newNode->ttl_queue = ttl;
    newNode->expiration = current_time_ms() + ttl;
    newNode->next = NULL;
    newNode->prev = NULL;
    return newNode;
}


void deleteNode(ElementListNode* node)
{
    // free everything else related to the node
    rm_free(node->element);
    rm_free(node);
}


//Creates a new Node and returns pointer to it.
ElementList* _createNewList() {
    ElementList* list
        = (ElementList*)rm_malloc(sizeof(ElementList));
    list->head = NULL;
    list->tail = NULL;
    list->len = 0;
    return list;
}


void deleteList(ElementList* list) {
    ElementListNode* current = list->head;

    // iterate over queue and find the element that has id = element_id
    while(current != NULL)
    {
        ElementListNode* next = current->next; // save next
        deleteNode(current);
        current = next;  //move to next node
    }

    rm_free(list);
}


// insert a Node at tail of linked list
void listPush(ElementList* list, ElementListNode* node)
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


void _listPull(Dehydrator* dehy, ElementList* list, ElementListNode* node)
{
    if (list == NULL || list == TRIEMAP_NOTFOUND)
        return;

    if (list->len == 1)
    {
        list->head = NULL;
        list->tail = NULL;
        if (dehy != NULL) {
            char ttl_str[256];
            sprintf(ttl_str, "%llu", node->ttl_queue);
            TrieMap_Delete(dehy->timeout_queues, ttl_str, strlen(ttl_str), NULL);
            // deleteList(list)
        }
        return;
    }

    //hot circuit the node (carefull when pulling from tail or head)
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


// pull and return the element at the first location
ElementListNode* _listPop(Dehydrator* dehy, ElementList* list) {
   if ((list == NULL) || (list->head == NULL)) { return NULL; } // if list empty

   //save current head
   ElementListNode* node = list->head;

   if (list->len == 1)
   {
       list->tail = NULL;
       list->head = NULL;
       if (dehy != NULL) {
            char ttl_str[256];
            sprintf(ttl_str, "%llu", node->ttl_queue);
            // TrieMap_Delete(dehy->timeout_queues, ttl_str, strlen(ttl_str), deleteList);
        }
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


ElementListNode* listPop(ElementList* list) {
    return _listPop(NULL, list);
}


/***************************
 *   Dehydrator Utilities
 ***************************/


Dehydrator* newDehydrator(void)
{

    Dehydrator* dehy
        = (Dehydrator*)rm_malloc(sizeof(Dehydrator));

    dehy->timeout_queues = NewTrieMap();
    dehy->element_nodes = NewTrieMap();

    return dehy;
}

void Dehydrator_Free(Dehydrator* dehy)
{
    TrieMap_Free(dehy->timeout_queues, deleteList);
    TrieMap_Free(dehy->element_nodes, NULL);

    rm_free(dehy);
}

void voidCB(void *elem) { return; }

void _removeNodeFromMapping(Dehydrator* dehydrator, ElementListNode* node)
{
    TrieMap_Delete(dehydrator->element_nodes, node->element, node->element_len, voidCB);
}


void * swapCB(void *oldval, void *newval){
    return newval;
} 

void _addNodeToMapping(Dehydrator* dehydrator, ElementListNode* node)
{
    //  printttNode(node);
    //  printf("");
    if (node != NULL)
        TrieMap_Add(dehydrator->element_nodes, node->element, node->element_len, node , swapCB);
    // printf("XXXXXXXXX\n");
}

/************************************
 * 
 *           DS handling
 * 
 ************************************/

/*
 * @return the number of uniqe ttl entries in the dehydrator
 */
size_t ttl_count(Dehydrator* dehy){
    return dehy->element_nodes->cardinality;
}

/*
 * Insert ttl for a new key or update an existing one
 * @return DHY_OK on success, DHY_ERR on error
 */
int set_element_ttl(Dehydrator* dehy, char* element, size_t len, mstime_t ttl_ms){
    char ttl_str[256];
    
    ElementListNode* node = TrieMap_Find(dehy->element_nodes, element, len);

    if (node != NULL && node != TRIEMAP_NOTFOUND){
        // remove existing node
        sprintf(ttl_str, "%llu", node->ttl_queue);
        ElementList* list = TrieMap_Find(dehy->timeout_queues, ttl_str, strlen(ttl_str));
        _listPull(dehy, list, node);
        _removeNodeFromMapping(dehy, node);
        deleteNode(node);
    } 

    //create new node
    ElementListNode* new_node = NewNode(element, len, ttl_ms);
    // printttNode(new_node);
    // put node in correct ttl queue
    sprintf(ttl_str, "%llu", new_node->ttl_queue);
    ElementList* new_list = TrieMap_Find(dehy->timeout_queues, ttl_str, strlen(ttl_str));
    if (new_list == NULL || new_list == TRIEMAP_NOTFOUND){
        // create new ttl queue
        new_list = _createNewList();
        TrieMap_Add(dehy->timeout_queues, ttl_str, strlen(ttl_str), new_list, NULL);
    }
    listPush(new_list, new_node);
    // printf("YYYYYYYYY\n");
    _addNodeToMapping(dehy, new_node);
    // printf("ZZZZZZZZZZZ\n");
    return DHY_OK;
   
}


/*
 * Get the expiration value for the given key
 * @return datetime of expiration (in milliseconds) on success, -1 on error
 */
mstime_t get_element_exp(Dehydrator* dehy, char* key){
    ElementListNode* node = TrieMap_Find(dehy->element_nodes, key, strlen(key));

    if (node != NULL && node != TRIEMAP_NOTFOUND)
        return node->expiration;
    return -1;
}


/*
 * Remove TTL from the dehydrator for the given key
 * @return DHY_OK
 */
int del_element_exp(Dehydrator* dehy, char* key){
    char ttl_str[256];
    ElementListNode* node = TrieMap_Find(dehy->element_nodes, key, strlen(key));

    if (node != NULL && node != TRIEMAP_NOTFOUND){
        // remove existing node
        sprintf(ttl_str, "%llu", node->ttl_queue);
        ElementList* list = TrieMap_Find(dehy->timeout_queues, ttl_str, strlen(ttl_str));
        _listPull(dehy, list, node);
        _removeNodeFromMapping(dehy, node);
        deleteNode(node);
    } 

    return DHY_OK;
}


ElementListNode* _get_next_node(Dehydrator* dehy){
    //  printf("IN NEXT NODE\n");
    TrieMapIterator * itr = TrieMap_Iterate(dehy->timeout_queues, "", 0);
    char* queue_name;
    tm_len_t len;
    ElementList* list;
    ElementListNode* retval = NULL;
    while (TrieMapIterator_Next(itr, &queue_name, &len, &list)) {
        if (list != NULL && list->len > 0 && list->head != NULL) {
            // printf("LOOKING AT %s\n", list->head->element);
            if ((retval == NULL) || (list->head->expiration < retval->expiration))
                retval = list->head;
        }
    }
    TrieMapIterator_Free(itr);
    return retval;
}

/*
 * @return the closest element expiration datetime (in milliseconds), or -1 if DS is empty
 */
mstime_t next_at(Dehydrator* dehy){
    ElementListNode* next_node = _get_next_node(dehy);
    if (next_node != NULL)
        return next_node->expiration;
    return -1;
}


/*
 * Remove the element with the closest expiration datetime from the dehydrator and return it's key
 * @return a pointer to a new string containng the key of the element with closest 
 * expiration datetime or NULL if the dehydrator is empty.
 */
char* pop_next(Dehydrator* dehy) {
    ElementListNode* next_node = _get_next_node(dehy);
        
    if (next_node != NULL) {
        char ttl_str[256];
        sprintf(ttl_str, "%llu", next_node->ttl_queue);
        ElementList* list = TrieMap_Find(dehy->timeout_queues, ttl_str, strlen(ttl_str));
        char* retval = strndup(next_node->element, next_node->element_len);
        _listPull(dehy, list, next_node);
        _removeNodeFromMapping(dehy, next_node);
        deleteNode(next_node);
        return retval;
    }
    return NULL;
}

ElementList* pop_expired(Dehydrator* dehy) {
    TrieMapIterator * itr = TrieMap_Iterate(dehy->timeout_queues, "", 0);
    char* queue_name;
    tm_len_t len;
    ElementList* list;
    ElementList* retval = _createNewList();
    mstime_t now = current_time_ms() + DHY_LATANCY_MS;
    while (TrieMapIterator_Next(itr, &queue_name, &len, &list)) {
        while (list != NULL && list->len > 0 && list->head != NULL) {
            if ((retval == NULL) || (list->head->expiration <= now))
                listPush(retval, _listPop(dehy, list));
        }
    }
    TrieMapIterator_Free(itr);
    return retval;
}

