/*
 * Websearch - docid.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * Functions and utilities for the generation of the DOCID and general URL handling
 */

#include "docid.h"
#include "hash.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

docid_t
docid_make( const char* buf, size_t len ) {
    // For now, all we do is hash the buffer...
    return murmur64A( buf, len, DOCID_HASH_SEED );
}

char*
docid_tostr( docid_t id ) {
    // A DOCID is 16 bytes long, but we add a null
    char* str =malloc( sizeof(char) * 17 );
    snprintf( str, 17, "%Lx", id );
    return str;
}

size_t
docid_sanitizeUrl( char* buf, size_t bufsize ) {
    // This function need to be vastly expanded in production code
    // For now, we only do some trivial stuff
    int i =0;
    enum urlpart_t {
        protocol,
        scheme,
        port,
        path,
        args
    } url;

    url =protocol;

    while( i < bufsize && buf[i] != 0 ) {
        switch( url ) {
            case protocol:
                if( buf[i] == ':' ) {
                    char *tmp =strstr( &buf[i], "//" );
                    if( tmp == NULL ) {
                        // malformed URL
                        return 0;
                    }
                    i+=2;
                    url =scheme;
                }
                else
                    buf[i] =tolower(buf[i]);
                break;
            case scheme: 
                if( buf[i] == '/' )
                    url =path;
                else if( buf[i] == ':' )
                    url =port;
                else
                    buf[i] =tolower(buf[i]);
                break;
            case port:
                if( strncmp( &buf[i], "80/", 3 ) == 0 ) {
                    memcpy( &buf[i-1], &buf[i+2], bufsize - (i+3) );
                    url =path;
                    bufsize -=3;
                }
                else {
                    while( i < bufsize && buf[i] != '/' ) i++;
                    url =path;
                }
                break;
            default: 
                break;
        }
        i++;
    }
    if( url == protocol )
        return 0; // malformed url
    return i;
}

