/*
 * Websearch - ranklist.h
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * Implements a simple associative array that stores <docid, rank> tuples 
 */

#include "ranklist.h"
#include <assert.h>
#define _GNU_SOURCE
#include <stdlib.h>

#define RANKLIST_ARRAYSIZE_INCR 1024

static int
rank_compare( const void* left, const void* right ) {
    rankelem_t* left_e =(rankelem_t*)left;
    rankelem_t* right_e =(rankelem_t*)right;

    if( left_e->rank > right_e->rank )
        return -1;
    if( left_e->rank < right_e->rank )
        return 1;

    return 0;
}

int 
ranklist_idxWeight( index_t idx ) {
    switch( idx ) {
        case IDX_WEBIDX:
            return 4;
        case IDX_TITLEIDX:
            return 16;
        default: break;
    }
    return 1;
}

void
ranklist_create( ranklist_t* r ) {
    r->count =0;
    r->first =malloc( sizeof( rankelem_t ) * RANKLIST_ARRAYSIZE_INCR );
    r->size =RANKLIST_ARRAYSIZE_INCR;
}

void 
ranklist_free( ranklist_t* r ) {
    free( r->first );
    r->size =r->count =0;
}

void
ranklist_push( ranklist_t* r, docid_t docid, index_t idx ) {
    // First, check if the docid already exists
    // TODO: add a separate bin. tree to do this faster
    rankelem_t *elem =NULL;
    for( size_t i =0; i < r->count; i++ ) {
        if( r->first[i].docid == docid ) {
            elem =r->first + i;
            break;
        }
    }

    if( elem ) {
        elem->rank +=ranklist_idxWeight( idx );
        return;
    }


    // Make more room if neccesary
    if( r->count == r->size ) {
        r->first =realloc( r->first, (r->size += RANKLIST_ARRAYSIZE_INCR) );
        assert( r->first != NULL );
    }
    elem =r->first + r->count;
    elem->docid =docid;
    elem->rank =ranklist_idxWeight( idx );

    r->count++;
}

void
ranklist_sort( ranklist_t* r ) {
    qsort( (void*)r->first, (size_t)r->count, sizeof( rankelem_t ), rank_compare );
}
