#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ll_node_t {
    void* data;             /* the actual data */
    struct ll_node_t* next; /* the next node */
} ll_node_t;

typedef struct linked_list_t {
    ll_node_t* head;            /* the head of the list */
    unsigned int data_size;     /* the size of stored data */
    unsigned int size;          /* the size of the list */
} linked_list_t;

/*
 *  Function to create a linked list
 *  
 * @param data_size the size of stored data
 * 
 * @return pointer to the new linked list
 */
linked_list_t *ll_create(unsigned int data_size);


/*
 * Function to add a new node to an existing list
 *
 * @param list the list to which the new node will be added
 * @param n the index at which the new node will be added
 * @param new_data a pointer to the data to be added
 */
void ll_add_nth_node(linked_list_t* list, unsigned int n, const void* new_data);


/*
 * Function to remove a node from an existing list
 *
 * @param list the list from which a node will be removed
 * @param n the index of the node to be removed
 * 
 * @return pointer to the removed node
 */
ll_node_t *ll_remove_nth_node(linked_list_t* list, unsigned int n);


/*
 * Function to get the size of an existing list
 *
 * @param list the list
 * 
 * @return the size of the given list
 */
unsigned int ll_get_size(linked_list_t* list);


/*
 * Function to clear an existing list
 *
 * @param list the list that will be cleared
 */
void ll_free_elems(linked_list_t **list);

/*
 * Function to free a list along its nodes
 *
 * @param pp_list the list to be freed
 */
void ll_free(linked_list_t** pp_list);