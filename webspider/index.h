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

typedef enum {
    IDX_WEBIDX,
    IDX_LINKIDX,
    IDX_PAGEIDX,
    IDX_TITLEIDX
} index_t;

#define WEBIDX_PATH "webindex/"
#define LINKIDX_PATH "linkindex/"
#define PAGEIDX_PATH "pageindex/"
#define TITLEIDX_PATH "titleindex/"

#endif
