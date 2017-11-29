#ifndef DEHYDRATOR_LIB_H
#define DEHYDRATOR_LIB_H

#include "trie/triemap.h"
#include "millisecond_time.h"

#define DHY_OK 0
#define DHY_ERR 1

#define DHY_LATANCY_MS 1 // elements will be poped prematurly at most this time

/***************************
 *  Linked List Definitions
 ***************************/

typedef struct element_list_node{
    char* element;
    size_t element_len;
    mstime_t ttl_queue;
    mstime_t expiration;
    struct element_list_node* next;
    struct element_list_node* prev;
} ElementListNode;

typedef struct element_list{
    ElementListNode* head;
    ElementListNode* tail;
    size_t len;
} ElementList;

/***************************
 *  Dehydrator Definition
 ***************************/

typedef struct dehydrator{
    TrieMap * timeout_queues; //<ttl_queue,ElementList>
    TrieMap * element_nodes; //<element_id,node*>
} Dehydrator;


/***************************
 *     CONSTRUCTOR/ DESTRUCTOR
 ***************************/
Dehydrator* newDehydrator(void);

void Dehydrator_Free(Dehydrator* dehy);

/************************************
 *   General DS handling functions
 ************************************/

/*
 * @return the number of uniqe ttl entries in the dehydrator
 */
size_t ttl_count(Dehydrator* dehy);

/*
 * Insert ttl for a new key or update an existing one
 * @return DHY_OK on success, DHY_ERR on error
 */
int set_element_ttl(Dehydrator* dehy, char* key, size_t len, mstime_t ttl_ms);

/*
 * Get the expiration value for the given key
 * @return datetime of expiration (in milliseconds) on success, -1 on error
 */
mstime_t get_element_exp(Dehydrator* dehy, char* key);

/*
 * Remove TTL from the dehydrator for the given key
 * @return DHY_OK
 */
int del_element_exp(Dehydrator* dehy, char* key);

/*
 * @return the closest element expiration datetime (in milliseconds), or -1 if DS is empty
 */
mstime_t next_at(Dehydrator* dehy);

/*
 * Remove the element with the closest expiration datetime from the dehydrator and return it's key
 * @return the key of the element with closest expiration datetime
 */
char* pop_next(Dehydrator* dehy);

/*
 * @return a list of all exired element nodes.
 */
ElementList* pop_expired(Dehydrator* dehy);

/**********************
 *  
 *      LIST UTILS
 * 
 **********************/

void deleteList(ElementList* list);

void listPush(ElementList* list, ElementListNode* node);

ElementListNode* listPop(ElementList* list);



/***************************
 *  
 *     ELEMENT NODE UTILS
 * 
 ***************************/

ElementListNode* NewNode(char* element, size_t element_len, mstime_t ttl);

void deleteNode(ElementListNode* node);


#endif
