/*****************************************************
 *
 * this is a c implementation of the dehydration module
 *
******************************************************/
#ifndef LOWLEVEL_DEHY_IMPL_H
#define LOWLEVEL_DEHY_IMPL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "khash.h"
#include "redismodule.h"
#include "rmutil/util.h"


//##########################################################
//#
//#                     Linked List
//#
//#########################################################

typedef struct element_list_node{
    void* element;
    char* element_id;
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
//#                     Hash Table
//#
//#########################################################

KHASH_MAP_INIT_INT(16, ElementList*);

KHASH_MAP_INIT_STR(32, ElementListNode*);


//##########################################################
//#
//#                     Utilities
//#
//#########################################################

typedef struct dehydrator{
    khash_t(16) *timeout_queues; //<ttl,ElementList>
    khash_t(32) * element_nodes; //<element_id,node*>
} Dehydrator;


//##########################################################
//#
//#                     REDIS COMMANDS
//#
//#########################################################

int PushCommand_impl(char* dehydrator_name, const char* element_id, void* element, long long ttl);

void* PullCommand_impl(char* dehydrator_name, char* element_id);

ElementList* PollCommand_impl(char* dehydrator_name);

void* LookCommand_impl(char* dehydrator_name, char* element_id);

int DeleteCommand_impl(char* dehydrator_name);

int UpdateCommand_impl(char* dehydrator_name, char* element_id,  void* updated_element);

int TimeToNextCommand_impl(char* dehydrator_name);

#endif  /* LOWLEVEL_DEHY_IMPL_H */
