/*
 * Websearch - index.h
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * These functions are reponsible for storing to and retrieving from the different indices
 * The index_ functions are used by both the webspider and the webquery programs
 */

#ifndef INDEX_H
#define INDEX_H

#include <stdio.h>
#include "docid.h"

typedef enum {
    IDX_WEBIDX =0,
    IDX_LINKIDX =1,
    IDX_PAGEIDX =2,
    IDX_TITLEIDX =3,
    IDX_IMAGEIDX =4,
    IDX_REPOSITORY =5
} index_t;

typedef enum {
    IDX_OPEN_WRITE,
    IDX_OPEN_READ 
} idx_openmode_t;

static const char* IDX_PATH[] = {
    "webindex/",
    "linkindex/",
    "pageindex/",
    "titleindex/",
    "imageindex/",
    "repository/" };

/* Split `link' into its constituent words
   `buffer' will point to an array of null-terminated char*'s
   Return the number of words/buffers written. */
size_t
index_splitLink( char*** buffer, const char* link, size_t len );

/* Return 1 if `keyword' is a non-trivial one (e.g. not `http') */
int
index_isNonTrivialKeyword( const char* keyword );

/* Tokenize inner-text from an html element
   Returns pointer to the first non-empty token,
   *buffer is changed by removing non-letter characters and HTML-entities
   buffer points after the null-character of the first non-empty token */
char*
index_tokInner( char** buffer );

/* Open `keyword' in `idx' and return its FILE* */
FILE*
index_open( index_t idx, const char* keyword, idx_openmode_t mode );


/* Add new DOCIDs to the index `idx' with keyword `keyword'
   `ids' is assumed to be an array of docid_t and its length is given by `len' */
int
index_append( index_t idx, const char* keyword, const docid_t* ids, size_t len );

/* Append or create link index entry `link' and add `referer' to it */
int 
index_appendLinkidx( docid_t link, docid_t referer );

/* Split html-innertext `str' into tokens and add `link' to all these keywords in index `idx' */
int 
index_appendHtmlInner( index_t idx, docid_t link, char* str );

/* Append or create web index entries for all words in `link' and add `docid' to them */
int
index_appendWebidx( docid_t docid, const char* url, size_t len );

/* Create a repository file for `docid' containing `data' */
int
index_appendRepository( docid_t docid, const char* url, size_t url_len, const char* data, size_t len );

#endif
