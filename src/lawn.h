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
 
 
#ifndef DEHYDRATOR_LIB_H
#define DEHYDRATOR_LIB_H

#include "trie/triemap.h"
#include "utils/millisecond_time.h"

#define DHY_OK 0
#define DHY_ERR 1

#define DHY_LATANCY_MS 1 // elements will be poped prematurly at most this time

/***************************
 *  Linked Queue Definitions
 ***************************/

typedef struct element_queue_node{
    char* element;
    size_t element_len;
    mstime_t ttl_queue;
    mstime_t expiration;
    struct element_queue_node* next;
    struct element_queue_node* prev;
} ElementQueueNode;

typedef struct element_queue{
    ElementQueueNode* head;
    ElementQueueNode* tail;
    size_t len;
} ElementQueue;

/***************************
 *    Lawn Definition
 ***************************/

typedef struct lawn{
    TrieMap * timeout_queues; //<ttl_queue,ElementQueue>
    TrieMap * element_nodes; //<element_id,node*>
} Lawn;


/***************************
 * CONSTRUCTOR/ DESTRUCTOR
 ***************************/
Lawn* newLawn(void);

void freeLawn(Lawn* dehy);

/************************************
 *   General DS handling functions
 ************************************/

/*
 * @return the number of uniqe ttl entries in the lawn
 */
size_t ttl_count(Lawn* dehy);

/*
 * Insert ttl for a new key or update an existing one
 * @return DHY_OK on success, DHY_ERR on error
 */
int set_element_ttl(Lawn* dehy, char* key, size_t len, mstime_t ttl_ms);

/*
 * Get the expiration value for the given key
 * @return datetime of expiration (in milliseconds) on success, -1 on error
 */
mstime_t get_element_exp(Lawn* dehy, char* key);

/*
 * Remove TTL from the lawn for the given key
 * @return DHY_OK
 */
int del_element_exp(Lawn* dehy, char* key);

/*
 * @return the closest element expiration datetime (in milliseconds), or -1 if DS is empty
 */
mstime_t next_at(Lawn* dehy);

/*
 * Remove the element with the closest expiration datetime from the lawn and return it's key
 * @return the key of the element with closest expiration datetime
 */
char* pop_next(Lawn* dehy);

/*
 * @return a queue of all exired element nodes.
 */
ElementQueue* pop_expired(Lawn* dehy);



/**********************
 *  
 *      QUEUE UTILS
 * 
 **********************/

void freeQueue(ElementQueue* queue);

void queuePush(ElementQueue* queue, ElementQueueNode* node);

ElementQueueNode* queuePop(ElementQueue* queue);



/***************************
 *  
 *     ELEMENT NODE UTILS
 * 
 ***************************/

ElementQueueNode* NewNode(char* element, size_t element_len, mstime_t ttl);

void freeNode(ElementQueueNode* node);


#endif
