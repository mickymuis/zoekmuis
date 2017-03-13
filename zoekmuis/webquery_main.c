/*
 * Websearch - webquery_main.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 */

#include <stdio.h>
#include <string.h>
#include "docid.h"
#include "ranklist.h"
#include "index.h"

#define MAXKWSIZE 2014

int main( int argc, char** argv ) {
    char keyword[MAXKWSIZE];
    if( argc < 2 ) {
        FILE *file =fopen( "queryterms.txt", "r" );
        if( file == NULL ) return -1;
        fscanf( file, "%s", keyword );
        fclose( file );
    }
    else
        strncpy( keyword, argv[1], MAXKWSIZE );

    ranklist_t r;
    ranklist_create( &r );

    for( int idx =0; idx < 4; idx++ ) {
        FILE* file =index_open( (index_t)idx, keyword, IDX_OPEN_READ );
        if( file == NULL ) continue;

        docid_t docid;

        while( !feof( file ) ) {
            if( fread( &docid, sizeof( docid_t ), 1, file ) != 1 )
                break;
            ranklist_push( &r, docid, (index_t)idx );
        }
    }

    ranklist_sort( &r );

    for( int i =0; i < r.count; i++ ) {
        printf( "%#x ranking #%d\n", r.first[i].docid, r.first[i].rank );
    }

    ranklist_free( &r );
    return 0;



/*    char buf[1024];

    while( 1 ) {
        printf( "url?> " );
        //scanf( "%s", buf );
        fgets( buf, 1024, stdin );
        size_t oldsize =strlen( buf );
        size_t newsize = docid_sanitizeUrl( buf, 1024 );
        printf( "Sanitized: `%s' (old size %d, new size %d)\n", buf, oldsize, newsize );
        
        char** buf2;
        size_t n =index_splitLink( &buf2, buf, newsize );
        for( int i=0; i < n; i++ )
            printf( " %s", buf2[i] );
        printf( "\n" );
        char *tmp =buf, *tok;
        while( ( tok = index_tokInner( &tmp ) ) != NULL ) printf( "- `%s' \n", tok );
    }
    return 0;*/
}
