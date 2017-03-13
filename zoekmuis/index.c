/*
 * Websearch - index.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * These functions are reponsible for storing to and retrieving from the different indices
 * The index_ functions are used by both the webspider and the webquery programs
 */

#include "index.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

#include <string.h>

char *strsep(char **stringp, const char *delim);

#define TMP_SIZE 1024

size_t
index_splitLink( char ***buffer, const char* link, size_t len ) {

    size_t n =1; // number of words + 1
    char **elems =malloc( sizeof( char* ) );
    char *tmp =malloc( sizeof(char) * TMP_SIZE );
    size_t tmp_size =TMP_SIZE;
    size_t j =0;
    size_t k =0;

    for( size_t i =0; i < len; i++ ) {
        
        switch( link[i] ) {
            case '.':
            case '-':
            case ':':
            case '/':
            case '#':
            case '?':
                goto compl;
            default:
                goto lettr;
        }

lettr: // Normal letter, added to tmp buffer
        if( j+1 == tmp_size )
            tmp =realloc( tmp, sizeof(char) * (tmp_size += TMP_SIZE) );
        tmp[j++] = link[i];
        continue;
compl: // Word complete, add to array
        if( j == 0 )
            continue;
        elems = realloc( elems, sizeof(char*) * (++n) );

        elems[k] =malloc( sizeof(char) * (j+1) );
        memcpy( elems[k], tmp, j );
        elems[k][j] =0;
        k++;
        j=0;
        if( link[i] == '?' )
            break; // Skip url arguments for now
    }
    elems[n-1] =NULL;
    *buffer =elems;
    free( tmp );
    return n-1;
}

char*
index_tokInner( char** buffer ) {
    char *first =NULL;
    size_t i =0;
    while( 1 ) {
        // alpha-numeric character: convert to lower case
        if( isalnum((*buffer)[i]) ) {
            if( first == NULL )
                first =*buffer + i;
            (*buffer)[i] = tolower( (*buffer)[i] );
        }
        // HTML entity
        else if( (*buffer)[i] == '&' ) {
            int off =0;
            for( int j=1; j < 7; j++ ) {
                if( (*buffer)[i+j] == 0 )
                    break;
                if( (*buffer)[i+j] == ';' ) {
                    off =j;
                    break;
                }
            }
            if( first != NULL )
                (*buffer)[i] = 0;
            i +=off;
            if( first != NULL ){
                *buffer +=i+1;
                break;
            }

        }
        // End of string
        else if( (*buffer)[i] == 0 ) {
            *buffer +=i;
            break;
        }
        // Any other character is a delimeter
        else {
            if( first != NULL ) {
                // Exception to handle combined words 
                if( (*buffer)[i] == '-' ) { i++; continue; }

                (*buffer)[i] = 0;
                *buffer +=i+1;
                break;
            }
        }
        i++;
    }
    return first;
}

FILE*
index_open( index_t idx, const char* keyword, idx_openmode_t mode  ) {
    size_t path_len =strlen( keyword ) + strlen( IDX_PATH[idx] ) + 1;
    char *path =malloc( sizeof(char) * path_len );
    strcpy( path, IDX_PATH[idx] );
    strcat( path, keyword );
    FILE *file;
    if( mode == IDX_OPEN_READ )
        file =fopen( path, "rb" );
    else if( idx ==IDX_REPOSITORY )
        file =fopen( path, "wb" );
    else
        file =fopen( path, "ab" );
    free( path );
    return file;
}

int
index_append( index_t idx, const char* keyword, const docid_t* ids, size_t len ) {

    FILE* file =index_open( idx, keyword, IDX_OPEN_WRITE );
    if( file == NULL ) goto err;

    if( fwrite( (void*)ids, sizeof( docid_t ), len, file ) != len )
        goto err;

    fclose( file );
    return 0;

err:    
    fprintf( stderr, "index_append(): %s\n",strerror( errno ) );
    return -1;
}

int 
index_appendLinkidx( docid_t link, docid_t referer ) {
    char *docid_str =docid_tostr( link );
    int err =index_append( IDX_LINKIDX, docid_str, &referer, 1 );

    free( docid_str );
    return err;
}

int 
index_appendHtmlInner( index_t idx, docid_t link, char* str ) {
    char *word = NULL;
    int err =0;

    int i=0;

    while( word = index_tokInner( &str ) ) {
        if( strlen( word ) == 0 )
            continue;
        if( err =index_append( idx, word, &link, 1 ) != 0 )
            break;
         
    }
    return err;
}

int
index_appendWebidx( docid_t docid, const char* url, size_t len ) {
    char **buffer;
    size_t n =index_splitLink( &buffer, url, len );
    int err =0;

    for( int i =0; i < n; i++ ) {
        if( !err )
                err = index_append( IDX_WEBIDX, buffer[i], &docid, 1 );
        free( buffer[i] );
    }

    free( buffer );
    return err;
}

int
index_appendRepository( docid_t docid, const char* url, size_t url_len, const char* data, size_t len ) {
    
    char *docid_str =docid_tostr( docid );

    FILE* file =index_open( IDX_REPOSITORY, docid_str, IDX_OPEN_WRITE );
    if( file == NULL ) goto err;

    // Write the URL and a newline first, this provides a simple means of reversing DOCIDs
    if( fwrite( (void*)url, sizeof( char ), url_len, file ) != url_len )
        goto err;
    
    if( fwrite( (void*)"\n", sizeof( char ), 1, file ) != 1 )
        goto err;

    // Now write the body of the page
    if( fwrite( (void*)data, sizeof( char ), len, file ) != len )
        goto err;

    free( docid_str );
    fclose( file );
    return 0;

err:    
    fprintf( stderr, "index_appendRepository(): %s\n",strerror( errno ) );
    return -1;

}

