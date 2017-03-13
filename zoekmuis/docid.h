/*
 * Websearch - docid.h
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * Functions and utilities for the generation of the DOCID and general URL handling
 */

#ifndef DOCID_H
#define DOCID_H

#define DOCID_HASH_SEED 0x1234567890ABCDEFl

#include <stdint.h>
#include <stdio.h>

typedef uint64_t docid_t;

docid_t
docid_make( const char* buf, size_t len );

char*
docid_tostr( docid_t id );

size_t
docid_sanitizeUrl( char* buf, size_t bufsize );


#endif

