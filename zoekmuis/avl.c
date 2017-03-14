/*
 * Websearch - avl.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * All code in this file was taken from:
 * http://www.zentut.com/c-tutorial/c-avl-tree/
 * with some slight modifications from me.
 */

#include "avl.h"
#include <malloc.h>
#include <stdio.h>
#include <assert.h>

/*
    remove all avl_node_ts of an AVL tree
*/
void avl_dispose(avl_node_t* t)
{
    if( t != NULL )
    {
        avl_dispose( t->left );
        avl_dispose( t->right );
        free( t );
    }
}
 
/*
    find a specific avl_node_t's key in the tree
*/
avl_node_t* avl_find(uint64_t e, avl_node_t* t )
{
    if( t == NULL )
        return NULL;
    if( e < t->data )
        return avl_find( e, t->left );
    else if( e > t->data )
        return avl_find( e, t->right );
    else
        return t;
}
 
/*
    find minimum avl_node_t's key
*/
avl_node_t* avl_find_min( avl_node_t* t )
{
    if( t == NULL )
        return NULL;
    else if( t->left == NULL )
        return t;
    else
        return avl_find_min( t->left );
}
 
/*
    find maximum avl_node_t's key
*/
avl_node_t* avl_find_max( avl_node_t* t )
{
    if( t != NULL )
        while( t->right != NULL )
            t = t->right;
 
    return t;
}
 
/*
    get the height of a avl_node_t
*/
static int height( avl_node_t* n )
{
    if( n == NULL )
        return -1;
    else
        return n->height;
}
 
/*
    get maximum value of two integers
*/
static int max( int l, int r)
{
    return l > r ? l: r;
}
 
/*
    perform a rotation between a k2 avl_node_t and its left child
 
    note: call single_rotate_with_left only if k2 avl_node_t has a left child
*/
 
static avl_node_t* single_rotate_with_left( avl_node_t* k2 )
{
    avl_node_t* k1 = NULL;
 
    k1 = k2->left;
    k2->left = k1->right;
    k1->right = k2;
 
    k2->height = max( height( k2->left ), height( k2->right ) ) + 1;
    k1->height = max( height( k1->left ), k2->height ) + 1;
    return k1; /* new root */
}
 
/*
    perform a rotation between a avl_node_t (k1) and its right child
 
    note: call single_rotate_with_right only if
    the k1 avl_node_t has a right child
*/
 
static avl_node_t* single_rotate_with_right( avl_node_t* k1 )
{
    avl_node_t* k2;
 
    k2 = k1->right;
    k1->right = k2->left;
    k2->left = k1;
 
    k1->height = max( height( k1->left ), height( k1->right ) ) + 1;
    k2->height = max( height( k2->right ), k1->height ) + 1;
 
    return k2;  /* New root */
}
 
/*
 
    perform the left-right double rotation,
 
    note: call double_rotate_with_left only if k3 avl_node_t has
    a left child and k3's left child has a right child
*/
 
static avl_node_t* double_rotate_with_left( avl_node_t* k3 )
{
    /* Rotate between k1 and k2 */
    k3->left = single_rotate_with_right( k3->left );
 
    /* Rotate between K3 and k2 */
    return single_rotate_with_left( k3 );
}
 
/*
    perform the right-left double rotation
 
   notes: call double_rotate_with_right only if k1 has a
   right child and k1's right child has a left child
*/
 
 
 
static avl_node_t* double_rotate_with_right( avl_node_t* k1 )
{
    /* rotate between K3 and k2 */
    k1->right = single_rotate_with_left( k1->right );
 
    /* rotate between k1 and k2 */
    return single_rotate_with_right( k1 );
}
 
/*
    insert a new avl_node_t into the tree
*/
avl_node_t* avl_insert(uint64_t e, avl_node_t* t )
{
    if( t == NULL )
    {
        /* Create and return a one-avl_node_t tree */
        t = (avl_node_t*)malloc(sizeof(avl_node_t));
        if( t == NULL )
        {
            fprintf (stderr, "Out of memory!!! (insert)\n");
            assert(0);
        }
        else
        {
            t->data = e;
            t->height = 0;
            t->left = t->right = NULL;
        }
    }
    else if( e < t->data )
    {
        t->left = avl_insert( e, t->left );
        if( height( t->left ) - height( t->right ) == 2 )
            if( e < t->left->data )
                t = single_rotate_with_left( t );
            else
                t = double_rotate_with_left( t );
    }
    else if( e > t->data )
    {
        t->right = avl_insert( e, t->right );
        if( height( t->right ) - height( t->left ) == 2 )
            if( e > t->right->data )
                t = single_rotate_with_right( t );
            else
                t = double_rotate_with_right( t );
    }
    /* Else X is in the tree already; we'll do nothing */
 
    t->height = max( height( t->left ), height( t->right ) ) + 1;
    return t;
}
 
/*
    data data of a avl_node_t
*/
uint64_t avl_get(avl_node_t* n)
{
    return n->data;
}
 
/*
    Recursively display AVL tree or subtree
*/
void avl_display(avl_node_t* t)
{
    if (t == NULL)
        return;
    printf("%Lx",(long long unsigned int)t->data);
 
    if(t->left != NULL)
        printf("(L:%Lx)",(long long unsigned int)t->left->data);
    if(t->right != NULL)
        printf("(R:%Lx)",(long long unsigned int)t->right->data);
    printf("\n");
 
    avl_display(t->left);
    avl_display(t->right);
}
