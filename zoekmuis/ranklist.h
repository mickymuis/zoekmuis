/*
 * Websearch - ranklist.h
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * Implements a simple associative array that stores <docid, rank> tuples 
 */

#ifndef RANKLIST_H
#define RANKLIST_H

#include "docid.h"
#include "index.h"

typedef struct rankelem {
    docid_t docid;
    int rank;
} rankelem_t;

typedef struct ranklist {
    rankelem_t *first;  // Pointer to the first element
    size_t count;       // Actual number of elements
    size_t size;        // Total array size
} ranklist_t;

int 
ranklist_idxWeight( index_t idx );

void
ranklist_create( ranklist_t* r );

void 
ranklist_free( ranklist_t* r );

void
ranklist_push( ranklist_t* r, docid_t docid, index_t idx );

void
ranklist_sort( ranklist_t* r );

#endif

