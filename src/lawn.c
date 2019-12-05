/* ###################################################################
 *  Lawn - Low Latancy Timer Data-Structure for Large Scale Systems 
 *  
 *  Autor: Adam Lev-Libfeld (adam@tamarlabs.com) 2017 
 *
 *  TL;DR - A Lawn is a timer data store, not unlike Timer-Wheel, but with 
 *  unlimited timer span with no degrigation in performance over a large set of timers.
 *
 *  Lawn is a high troughput data structure that is based on the assumption 
 *  that most timers are set to a small set of TTLs to boost overall DS 
 *  performance. It can assist when handling a large set of timers with 
 *  relativly small variance in TTL by effectivly using minimal queues 
 *  to store timer data. Achieving O(1) for insertion and deletion of timers, 
 *  and O(1) for tiemr expiration.
 *
 *  Lawn is distributed Under the Apache2 licence 
 *
 * ################################################################### */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include "lawn.h"
#include "utils/millisecond_time.h"
#include "trie/triemap.h"



/***************************
 *    Trie Utilities
 ***************************/


void voidCB(void *elem) { return; }

void * swapCB(void *oldval, void *newval){ return newval; }

void freeQueueCB(void *elem) { freeQueue((ElementQueue*)elem); }


/***************************
 *       Queue Utilities
 ***************************/

//Creates a new Node and returns pointer to it.
ElementQueueNode* NewNode(char* element, size_t element_len, mstime_t ttl)
{
    ElementQueueNode* newNode
        = (ElementQueueNode*)malloc(sizeof(ElementQueueNode));
    newNode->element = strndup(element, element_len);
    newNode->element_len = element_len;
    newNode->ttl_queue = ttl;
    newNode->expiration = current_time_ms() + ttl;
    newNode->next = NULL;
    newNode->prev = NULL;
    return newNode;
}


void freeNode(ElementQueueNode* node)
{
    // free everything else related to the node
    free(node->element);
    free(node);
}


//Creates a new Node and returns pointer to it.
ElementQueue* newQueue() {
    ElementQueue* queue
        = (ElementQueue*)malloc(sizeof(ElementQueue));
    queue->head = NULL;
    queue->tail = NULL;
    queue->len = 0;
    return queue;
}


void freeQueue(ElementQueue* queue) {
    ElementQueueNode* current = queue->head;

    // iterate over queue and find the element that has id = element_id
    while(current != NULL)
    {
        ElementQueueNode* next = current->next; // save next
        freeNode(current);
        current = next;  //move to next node
    }

    free(queue);
}


// insert a Node at tail of linked queue
void queuePush(ElementQueue* queue, ElementQueueNode* node)
{
    if (queue->tail == NULL)
    {
        queue->head = node;
    }
    else
    {
        node->prev = queue->tail;
        queue->tail->next = node;
    }
    queue->tail = node;
    queue->len = (queue->len) + 1;
}


void _queuePull(Lawn* lawn, ElementQueue* queue, ElementQueueNode* node)
{
    if (queue == NULL || queue == TRIEMAP_NOTFOUND)
        return;

    if (queue->len == 1)
    {
        queue->head = NULL;
        queue->tail = NULL;
        if (lawn != NULL) {
            char ttl_str[256];
            sprintf(ttl_str, "%llu", node->ttl_queue);
            TrieMap_Delete(lawn->timeout_queues, ttl_str, strlen(ttl_str), freeQueueCB);
        }
        return;
    }

    //hot circuit the node (carefull when pulling from tail or head)
    if (node == queue->head)
    {
        queue->head = queue->head->next;
        queue->head->prev = NULL;
    }
    else
    {
        node->prev->next = node->next;
    }

    if (node == queue->tail) {
        queue->tail = node->prev;
        queue->tail->next = NULL;
    }
    else
    {
        node->next->prev = node->prev;
    }
    queue->len = queue->len - 1;
}


// pull and return the element at the first location
ElementQueueNode* _queuePop(Lawn* lawn, ElementQueue* queue) {
   if ((queue == NULL) || (queue->head == NULL)) { return NULL; } // if queue empty

   //save current head
   ElementQueueNode* node = queue->head;

   if (queue->len == 1)
   {
       queue->tail = NULL;
       queue->head = NULL;
       if (lawn != NULL) {
            char ttl_str[256];
            sprintf(ttl_str, "%llu", node->ttl_queue);
            // TrieMap_Delete(lawn->timeout_queues, ttl_str, strlen(ttl_str), freeQueue);
        }
   }
   else
   {
       // swap to new head
       queue->head = queue->head->next;
       queue->head->prev = NULL;
   }

   queue->len = queue->len - 1;
   return node;

}


ElementQueueNode* queuePop(ElementQueue* queue) {
    return _queuePop(NULL, queue);
}


/***************************
 *   Lawn Utilities
 ***************************/


Lawn* newLawn(void)
{

    Lawn* lawn
        = (Lawn*)malloc(sizeof(Lawn));

    lawn->timeout_queues = NewTrieMap();
    lawn->element_nodes = NewTrieMap();

    return lawn;
}

void freeLawn(Lawn* lawn)
{
    TrieMap_Free(lawn->element_nodes, voidCB);
    TrieMap_Free(lawn->timeout_queues, freeQueueCB);

    free(lawn);
}

void _removeNodeFromMapping(Lawn* lawn, ElementQueueNode* node)
{
    TrieMap_Delete(lawn->element_nodes, node->element, node->element_len, voidCB);
}

void _addNodeToMapping(Lawn* lawn, ElementQueueNode* node)
{
    if (node != NULL)
        TrieMap_Add(lawn->element_nodes, node->element, node->element_len, node , swapCB);
}

/************************************
 * 
 *           DS handling
 * 
 ************************************/

/*
 * @return the number of uniqe ttl entries in the lawn
 */
size_t ttl_count(Lawn* lawn){
    return lawn->element_nodes->cardinality;
}

/*
 * Insert ttl for a new key or update an existing one
 * @return DHY_OK on success, DHY_ERR on error
 */
int set_element_ttl(Lawn* lawn, char* element, size_t len, mstime_t ttl_ms){
    char ttl_str[256];
    
    ElementQueueNode* node = TrieMap_Find(lawn->element_nodes, element, len);

    if (node != NULL && node != TRIEMAP_NOTFOUND){
        // remove existing node
        sprintf(ttl_str, "%llu", node->ttl_queue);
        ElementQueue* queue = TrieMap_Find(lawn->timeout_queues, ttl_str, strlen(ttl_str));
        _queuePull(lawn, queue, node);
        _removeNodeFromMapping(lawn, node);
        freeNode(node);
    } 

    //create new node
    ElementQueueNode* new_node = NewNode(element, len, ttl_ms);
    // put node in correct ttl queue
    sprintf(ttl_str, "%llu", new_node->ttl_queue);
    ElementQueue* new_queue = TrieMap_Find(lawn->timeout_queues, ttl_str, strlen(ttl_str));
    if (new_queue == NULL || new_queue == TRIEMAP_NOTFOUND){
        // create new ttl queue
        new_queue = newQueue();
        TrieMap_Add(lawn->timeout_queues, ttl_str, strlen(ttl_str), new_queue, NULL);
    }
    queuePush(new_queue, new_node);
    _addNodeToMapping(lawn, new_node);
    return DHY_OK;
   
}


/*
 * Get the expiration value for the given key
 * @return datetime of expiration (in milliseconds) on success, -1 on error
 */
mstime_t get_element_exp(Lawn* lawn, char* key){
    ElementQueueNode* node = TrieMap_Find(lawn->element_nodes, key, strlen(key));

    if (node != NULL && node != TRIEMAP_NOTFOUND)
        return node->expiration;
    return -1;
}


/*
 * Remove TTL from the lawn for the given key
 * @return DHY_OK
 */
int del_element_exp(Lawn* lawn, char* key){
    char ttl_str[256];
    ElementQueueNode* node = TrieMap_Find(lawn->element_nodes, key, strlen(key));

    if (node != NULL && node != TRIEMAP_NOTFOUND){
        // remove existing node
        sprintf(ttl_str, "%llu", node->ttl_queue);
        ElementQueue* queue = TrieMap_Find(lawn->timeout_queues, ttl_str, strlen(ttl_str));
        _queuePull(lawn, queue, node);
        _removeNodeFromMapping(lawn, node);
        freeNode(node);
    } 

    return DHY_OK;
}


ElementQueueNode* _get_next_node(Lawn* lawn){
    TrieMapIterator * itr = TrieMap_Iterate(lawn->timeout_queues, "", 0);
    char* queue_name;
    tm_len_t len;
    void* queue_pointer;
    ElementQueueNode* retval = NULL;
    while (TrieMapIterator_Next(itr, &queue_name, &len, &queue_pointer)) {
        ElementQueue* queue = (ElementQueue*)queue_pointer;
        if (queue != NULL && queue->len > 0 && queue->head != NULL) {
            if ((retval == NULL) || (queue->head->expiration < retval->expiration))
                retval = queue->head;
        }
    }
    TrieMapIterator_Free(itr);
    return retval;
}

/*
 * @return the closest element expiration datetime (in milliseconds), or -1 if DS is empty
 */
mstime_t next_at(Lawn* lawn){
    ElementQueueNode* next_node = _get_next_node(lawn);
    if (next_node != NULL)
        return next_node->expiration;
    return -1;
}


/*
 * Remove the element with the closest expiration datetime from the lawn and return it's key
 * @return a pointer to a new string containng the key of the element with closest 
 * expiration datetime or NULL if the lawn is empty.
 */
char* pop_next(Lawn* lawn) {
    ElementQueueNode* next_node = _get_next_node(lawn);
        
    if (next_node != NULL) {
        char ttl_str[256];
        sprintf(ttl_str, "%llu", next_node->ttl_queue);
        ElementQueue* queue = TrieMap_Find(lawn->timeout_queues, ttl_str, strlen(ttl_str));
        char* retval = strndup(next_node->element, next_node->element_len);
        _queuePull(lawn, queue, next_node);
        _removeNodeFromMapping(lawn, next_node);
        freeNode(next_node);
        return retval;
    }
    return NULL;
}

ElementQueue* pop_expired(Lawn* lawn) {
    TrieMapIterator * itr = TrieMap_Iterate(lawn->timeout_queues, "", 0);
    char* queue_name;
    tm_len_t len;
    void* queue_pointer;
    ElementQueue* retval = newQueue();
    mstime_t now = current_time_ms() + DHY_LATANCY_MS;
    while (TrieMapIterator_Next(itr, &queue_name, &len, &queue_pointer)) {
        ElementQueue* queue = (ElementQueue*)queue_pointer;
        while (queue != NULL && queue->len > 0 && queue->head != NULL) {
            if ((retval == NULL) || (queue->head->expiration <= now))
                queuePush(retval, _queuePop(lawn, queue));
            else
                break;
        }
    }
    TrieMapIterator_Free(itr);
    return retval;
}

