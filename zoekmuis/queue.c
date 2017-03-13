/*
 * Websearch - queue.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * I've taken apart the queue functions from the assignment code 
 * and created a more generalized and robust implementation.
 */

#include "queue.h"
#include "docid.h"
#include <malloc.h>
#include <math.h>
#include <assert.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int
queue_create( queue_t* q, size_t size, char sep ) {
    q->size =size;
    q->data =(char*)malloc( size * sizeof( char ) );
    if( q->data == NULL )
        return QUEUE_ERR_BADALLOC;
    q->sep =sep;
    q->front =0;
    q->rear =0;
    q->count =0;
    q->avl_root =NULL;
    return 0;
}

int 
queue_push( queue_t* q, const char* buffer, size_t size, docid_t hash ) {
    // Added for doubles checking
    avl_node_t *n =avl_find( hash, q->avl_root ); 
    if( n != NULL ) {
        // ignore, already in queue
        //printf( "Ignoring '%s', already in queue\n", buffer );
        return 0;
    }


    if( (q->rear >= q->front && q->rear + size + 1 < q->size)
        || (q->rear < q->front && q->rear + size + 1 < q->front) ) {
        // buffer fits, that ok.
        memcpy( (void*)(q->data + q->rear), buffer, size );
        q->rear +=size;
        q->data[q->rear] =q->sep;
        q->rear++;
    } else if( q->front > size+1 && q->front <= q->rear ) {
        // we can sqeeuze to make room, also fine.
        memcpy( (void*)q->data, buffer, size );
        q->data[size] =q->sep;
        q->data[q->rear] =0; // Mark the 'turn around point'
        q->rear =size+1;
    }
    else {
        // the boat is full.
        return QUEUE_ERR_FULL;
    }
    q->count++;

    // Added for doubles checking
    q->avl_root = avl_insert( hash, q->avl_root );

    return 0;
}

void
queue_pop( queue_t* q ) {
    if( !q->count )
        return;
    size_t offs =q->front;
    while( q->data[offs] != q->sep ) {
        if( offs == q->size || q->data[offs] == 0 ) { // Wrap around
            offs =0;
            if( q->data[0] == q->sep ) break;
        }
        if( offs == q->rear ) { // Bump into rear. TODO: Array is obviously corrupted, throw meaningful error?
            assert(0);
        }
        offs++;
    }
    q->front = offs+1;
    q->count--;
    if( q->front == q->rear )
        q->front = q->rear =0;
}

int 
queue_getCurrent( queue_t* q, char* buf, size_t maxlen ) {
    if( !q->count )
        return 0;
    size_t offs =q->front;
    while( q->data[offs] != q->sep ) {
        if( offs == q->rear || offs == q->size ) { // TODO: Array is obviously corrupted, throw meaningful error?
            assert(0);
        }
        if( q->data[offs] == 0 ) { // Wrap around
            q->front =0;
            offs =0;
            if( q->data[0] == q->sep ) break;
        }
        offs++;
    }
    size_t len =MIN( maxlen-1, offs - q->front );
    memcpy( (void*)buf, (void*)(q->data + q->front), len );
    buf[len] ='\0';
    
    return len;
}

void
queue_free( queue_t* q ) {
    free( q->data );
    q->size =0;
}

size_t
queue_bytesInUse( queue_t* q ) {
    if( q->front > q->rear )
        return q->size - (q->front - q->rear);
    return q->rear - q->front;
}
