/*
 * Websearch - queue.h
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * I've taken apart the queue functions from the assignment code 
 * and created a more generalized and robust implementation.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h> 
#include <string.h>
#include "avl.h"

#define QUEUE_ERR_FULL -1
#define QUEUE_ERR_BADALLOC -2

typedef struct {
    char *data; // Pointer to the beginning of the queue
    size_t front; // Index of the front element's first address
    size_t rear; // One past the rear element's last address
    char sep; // Value used to separate elements within the queue
    size_t size; // Array size in bytes
    size_t count; // We keep count to avoid bruteforce counting of elements

    // Added for doubles checking
    avl_node_t* avl_root;
} queue_t;

int
queue_create( queue_t* q, size_t size, char sep );

int 
queue_push( queue_t* q, const char* buffer, size_t size );

void
queue_pop( queue_t* q );

int 
queue_getCurrent( queue_t* q, char* buf, size_t maxlen );

void
queue_free( queue_t* q );

size_t
queue_bytesInUse( queue_t* q );

#endif

