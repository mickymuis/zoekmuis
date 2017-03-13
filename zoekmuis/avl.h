/*
 * Websearch - avl.h
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * All code in this file was taken from:
 * http://www.zentut.com/c-tutorial/c-avl-tree/
 * with some slight modifications from me.
 */

#ifndef AVLTREE_H
#define AVLTREE_H

#include <stdint.h>

typedef struct avl_node
{
    uint64_t data;
    struct avl_node*  left;
    struct avl_node*  right;
    int      height;
} avl_node_t;
 
 
void avl_dispose(avl_node_t* t);
avl_node_t* avl_find( uint64_t e, avl_node_t *t );
avl_node_t* avl_find_min( avl_node_t *t );
avl_node_t* avl_find_max( avl_node_t *t );
avl_node_t* avl_insert( uint64_t data, avl_node_t *t );
void avl_display(avl_node_t* t);
uint64_t avl_get( avl_node_t* n );

#endif // AVLTREE_H
