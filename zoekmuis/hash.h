/*
 * Websearch - hash.h
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * All code in this file was written by Austin Appleby
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
 * with some slight modifications by me.
 */

#ifndef HASH_H
#define HASH_H

#include <stdint.h>

uint64_t murmur64A ( const void * key, int len, uint64_t seed );

#endif

