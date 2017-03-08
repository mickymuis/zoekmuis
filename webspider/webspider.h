/*
 * Websearch - webspider.h
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * Retrieving and parsing of webpages
 */
#ifndef WEBSPIDER_H
#define WEBSPIDER_H

#include <stdio.h> 
#include <string.h>
#include "queue.h"

/* Obtains the contents of url by means of synchronous I/O
   On success, *buffer and *effective_url will point to a buffer holding the contents
   and the actual absolute url followed by CURL, respectively.
   On success, *buffer and *effective_url need to be freed after use.
   Returns the length of *buffer on success and returns NULL on failure. */
size_t
get_webpage( char** buffer, char** effective_url, const char* url );

/* Given a htmlpage, parse and index the complete page.
   */
size_t
parse_webpage( char* htmlpage, size_t length, const char* abs_url, queue_t* q );


#endif

