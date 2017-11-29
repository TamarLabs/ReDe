/* This is a stand-alone implementation of a real-time expiration data store.
 * It is basically a min heap <key, exp_version> sorted by expiration,
 * with a map of [key] -> <exp_version, exp> on the side
 */
#include "libdehy.h"

#include "../millisecond_time.h"

#include <time.h>
#include <unistd.h>



/*############################################################################333*/


char* string_append(char* a, const char* b)
{
    char* retstr = malloc(strlen(a)+strlen(b));
    strcpy(retstr, a);
    strcat(retstr, b);
    // printf("printing: %s", retstr);
    free(a);
    return retstr;
}


char* printNode(ElementListNode* node)
{
    char* node_str = (char*)malloc((node->element_len+256+256)*sizeof(char));
    sprintf(node_str, "[elem=%s,ttl=%llu,exp=%llu]", node->element, node->ttl_queue, node->expiration);
    return node_str;

}


char* printList(ElementList* list)
{
    char* list_str = (char*)malloc(32*sizeof(char));
    ElementListNode* current = list->head;
    sprintf(list_str, "(elements=%ld)\n   head", list->len);
    // iterate over queue and find the element that has id = element_id
    while(current != NULL)
    {
        list_str = string_append(list_str, "->");
        char* node_str = printNode(current);
        list_str = string_append(list_str, node_str);
        free(node_str);

        current = current->next;  //move to next node
    }
    list_str = string_append(list_str, "\n   tail points to: ");
    list_str = string_append(list_str, list->tail->element);
    list_str = string_append(list_str,"\n");
    return list_str;
}

/*############################################################################333*/



#define SUCCESS 0
#define FAIL 1

// Dehydrator* newDehydrator(void);
// void Dehydrator_Free(Dehydrator* store);
int constructor_distructore_test() {
  Dehydrator* store = newDehydrator();
  Dehydrator_Free(store);
  return SUCCESS;
}

/*
 * Insert expiration for a new key or update an existing one
 * @return DHY_OK on success, DHY_ERR on error
 */
// int set_element_ttl(Dehydrator* store, char* key, mstime_t ttl_ms);
int test_set_element_ttl() {
  int retval = FAIL;
  mstime_t ttl_ms = 10000;
  mstime_t expected = current_time_ms() + ttl_ms;
  char* key = "set_get_test_key";
  Dehydrator* store = newDehydrator();
  if (set_element_ttl(store, key, strlen(key), ttl_ms) == DHY_ERR) return FAIL;
  retval = SUCCESS;

  Dehydrator_Free(store);
  return retval;
}


/*
 * Get the expiration value for the given key
 * @return datetime of expiration (in milliseconds) on success, -1 on error
 */
// mstime_t get_element_exp(Dehydrator* store, char* key);
int test_set_get_element_exp() {
  int retval = FAIL;
  mstime_t ttl_ms = 10000;
  mstime_t expected = current_time_ms() + ttl_ms;
  char* key = "set_get_test_key";
  Dehydrator* store = newDehydrator();
  if (set_element_ttl(store, key, strlen(key), ttl_ms) == DHY_ERR) return FAIL;
  mstime_t saved_ms = get_element_exp(store, key);
  if (saved_ms != expected) {
    printf("ERROR: expected %llu but found %llu\n", expected, saved_ms);
    retval = FAIL;
  } else
    retval = SUCCESS;

  Dehydrator_Free(store);
  return retval;
}

/*
 * Remove expiration from the data store for the given key
 * @return DHY_OK
 */
// int del_element_exp(Dehydrator* store, char* key);
int test_del_element_exp() {
  int retval = FAIL;
  mstime_t ttl_ms = 10000;
  mstime_t expected = -1;
  char* key = "del_test_key";
  Dehydrator* store = newDehydrator();
  if (set_element_ttl(store, key, strlen(key), ttl_ms) == DHY_ERR) return FAIL;
  if (del_element_exp(store, key) == DHY_ERR) return FAIL;
  mstime_t saved_ms = get_element_exp(store, key);
  if (saved_ms != expected) {
    printf("ERROR: expected %llu but found %llu\n", expected, saved_ms);
    retval = FAIL;
  } else
    retval = SUCCESS;

  Dehydrator_Free(store);
  return retval;
}

/*
 * @return the closest element expiration datetime (in milliseconds), or -1 if DS is empty
 */
// mstime_t next_at(Dehydrator* store);
int test_next_at() {
  int retval = FAIL;
  Dehydrator* store = newDehydrator();

  mstime_t ttl_ms1 = 10000;
  char* key1 = "next_at_test_key_1";

  mstime_t ttl_ms2 = 2000;
  char* key2 = "next_at_test_key_2";

  mstime_t ttl_ms3 = 3000;
  char* key3 = "next_at_test_key_3";

  mstime_t ttl_ms4 = 400000;
  char* key4 = "next_at_test_key_4";
//    set_element_ttl(store, key1, strlen(key1), ttl_ms1);

//    set_element_ttl(store, key2, strlen(key2), ttl_ms2);

//    set_element_ttl(store, key3, strlen(key3), ttl_ms3);
// // printf("XXXXTESTTESTTEST\n");
//    del_element_exp(store, key2);
//   set_element_ttl(store, key4, strlen(key4), ttl_ms4);
//     return SUCCESS;
  
  if ((set_element_ttl(store, key1, strlen(key1), ttl_ms1) != DHY_ERR) &&
      (set_element_ttl(store, key2, strlen(key2), ttl_ms2) != DHY_ERR) &&
      (set_element_ttl(store, key3, strlen(key3), ttl_ms3) != DHY_ERR) &&
      (del_element_exp(store, key2) != DHY_ERR) &&
      (set_element_ttl(store, key4, strlen(key4), ttl_ms4) != DHY_ERR)) {
    mstime_t expected = current_time_ms() + ttl_ms3;
    mstime_t saved_ms = next_at(store);
    if (saved_ms != expected) {
      printf("ERROR: expected %llu but found %llu\n", expected, saved_ms);
      retval = FAIL;
    } else
      retval = SUCCESS;
  }

  Dehydrator_Free(store);
  return retval;
}

/*
 * Remove the element with the closest expiration datetime from the data store and return it's key
 * @return the key of the element with closest expiration datetime
 */
// char* pop_next(Dehydrator* store);
int test_pop_next() {
  int retval = FAIL;
  Dehydrator* store = newDehydrator();

  mstime_t ttl_ms1 = 10000;
  char* key1 = "pop_next_test_key_1";

  mstime_t ttl_ms2 = 2000;
  char* key2 = "pop_next_test_key_2";

  mstime_t ttl_ms3 = 3000;
  char* key3 = "pop_next_test_key_3";

  if ((set_element_ttl(store, key1, strlen(key1), ttl_ms1) != DHY_ERR) &&
      (set_element_ttl(store, key2, strlen(key2), ttl_ms2) != DHY_ERR) &&
      (del_element_exp(store, key2) != DHY_ERR) &&
      (set_element_ttl(store, key3, strlen(key3), ttl_ms3) != DHY_ERR)) {

    char* expected = key3;
    char* actual = pop_next(store);
    if (strcmp(expected, actual)) {
      printf("ERROR: expected \'%s\' but found \'%s\'\n", expected, actual);
      retval = FAIL;
    } else {
      // make sure we actually delete the thing
      mstime_t expected_ms = -1;
      mstime_t saved_ms = get_element_exp(store, expected);
      if (expected_ms != saved_ms) {
        printf("ERROR: expected %llu but found %llu\n", expected_ms, saved_ms);
        retval = FAIL;
      } else
        retval = SUCCESS;
    }
    free(actual);
  }
  Dehydrator_Free(store);
  return retval;
}


// ElementList* pop_expired(Dehydrator* dehy)
int test_pop_expired() {
// ElementList* pop_expired(Dehydrator* dehy)
  int retval = FAIL;
  Dehydrator* store = newDehydrator();

  mstime_t ttl_ms1 = 10000;
  char* key1 = "pop_next_test_key_1";

  mstime_t ttl_ms2 = 2000;
  char* key2 = "pop_next_test_key_2";

  mstime_t ttl_ms3 = 3000;
  char* key3 = "pop_next_test_key_3";
  
  mstime_t ttl_ms4 = 4000;
  char* key4 = "pop_next_test_key_4";

  if ((set_element_ttl(store, key1, strlen(key1), ttl_ms1) != DHY_ERR) &&
      (set_element_ttl(store, key2, strlen(key2), ttl_ms2) != DHY_ERR) &&
      (set_element_ttl(store, key3, strlen(key3), ttl_ms3) != DHY_ERR) &&
      (del_element_exp(store, key2) != DHY_ERR) &&
      (set_element_ttl(store, key4, strlen(key4), ttl_ms4) != DHY_ERR)) {

    ElementList* list = pop_expired(store);
    if (list->len != 0){
      printf("ERROR: expected empty list but found to have %ld items\n", list->len);
      retval = FAIL;
    } else {
      deleteList(list);
      sleep(4);
      list = pop_expired(store);
      // printf("LIST: \n\n\n%s\n\n",printList(list));
      int expected_len = 2;
      if (list->len != expected_len){
        printf("ERROR: expected list of len %d but found to have %ld items\n", expected_len, list->len);
        retval = FAIL;
      } else {
        ElementListNode* node1 = listPop(list);
        char* expexted_elem1 = key3;
        ElementListNode* node2 = listPop(list);
        char* expexted_elem2 = key4;
        if (node1->element != expexted_elem1){
          printf("ERROR: expected element to contain %s but found %s\n", expexted_elem1, node1->element);
          retval = FAIL;
        }
        if (node1->element != expexted_elem2){
          printf("ERROR: expected element to contain %s but found %s\n", expexted_elem2, node2->element);
          retval = FAIL;
        }
        deleteNode(node1);
        deleteNode(node2);
      }
      deleteList(list);
      retval = SUCCESS;
      // TODO: continue check until list is empty again
    }
  }
  Dehydrator_Free(store);
  return retval;
}

int main(int argc, char* argv[]) {
  mstime_t start_time = current_time_ms();
  int num_of_failed_tests = 0;
  int num_of_passed_tests = 0;
  printf("-------------------\n  STARTING TESTS\n-------------------\n\n");

  printf("-> constructor-distructore test\n");
  if (constructor_distructore_test() == FAIL) {
    ++num_of_failed_tests;
    printf(" FAILED constructor-distructore\n");
  } else {
    printf(" PASSED\n");
    ++num_of_passed_tests;
  }

  printf("-> set test\n");
  if (test_set_element_ttl() == FAIL) {
    ++num_of_failed_tests;
    printf(" FAILED on set\n");
  } else {
    printf(" PASSED\n");
    ++num_of_passed_tests;
  }

  printf("-> set-get test\n");
  if (test_set_get_element_exp() == FAIL) {
    ++num_of_failed_tests;
    printf(" FAILED on set-get\n");
  } else {
    printf(" PASSED\n");
    ++num_of_passed_tests;
  }

  printf("-> del test\n");
  if (test_del_element_exp() == FAIL) {
    ++num_of_failed_tests;
    printf(" FAILED on del\n");
  } else {
    printf(" PASSED\n");
    ++num_of_passed_tests;
  }

  printf("-> pop_next test\n");
  if (test_pop_next() == FAIL) {
    ++num_of_failed_tests;
    printf(" FAILED on pop_next\n");
  } else {
    printf(" PASSED\n");
    ++num_of_passed_tests;
  }

  printf("-> next_at\n");
  if (test_next_at() == FAIL) {
    ++num_of_failed_tests;
    printf(" FAILED on next_at\n");
  } else {
    printf(" PASSED\n");
    ++num_of_passed_tests;
  }

  // if (test_pop_expired() == FAIL) {
  //   ++num_of_failed_tests;
  //   printf("FAILED on pop_expired\n");
  // } else {
  //   printf("PASSED pop_expired\n");
  //   ++num_of_passed_tests;
  // }

  double total_time_ms = current_time_ms() - start_time;
  printf("\n-------------\n");
  if (num_of_failed_tests) {
    printf("Failed (%d tests failed and %d passed in %.2f sec)\n", num_of_failed_tests,
           num_of_passed_tests, total_time_ms / 1000);
    return FAIL;
  } else {
    printf("OK (%d tests passed in %.2f sec)\n\n", num_of_passed_tests, total_time_ms / 1000);
    return SUCCESS;
  }
}
