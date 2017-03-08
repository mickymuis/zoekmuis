/*
 * Websearch - webquery_main.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 */

#include <stdio.h>
#include <string.h>
#include "docid.h"

int main( int argc, char** argv ) {
    char buf[1024];

    while( 1 ) {
        printf( "url?> " );
        scanf( "%s", buf );
        size_t oldsize =strlen( buf );
        size_t newsize = docid_sanitizeUrl( buf, 1024 );
        printf( "Sanitized: `%s' (old size %d, new size %d)\n", buf, oldsize, newsize );
    }
    return 0;
}
