/*
 * Websearch - webspider_main.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 */

#include "webspider.h"
#include "queue.h"
#include "docid.h"
#include "index.h"
#include <stdio.h>
#include <stdlib.h>

#define MAXQSIZE 9000000       // Maximum size of the queue, q
#define MAXURL 100000          // Maximum size of a URL
#define MAXDOWNLOADS 300      // Maximum number of downloads we will attempt

void 
show_help( const char *name ) {
    fprintf( stderr, "%s [url] - Crawl the web using BFS, starting at [url]\n", name );  
}

int
isValidDomain( const char* url ) {
      if((strstr(url,"leidenuniv.nl") != NULL) || (strstr(url,"liacs.nl") != NULL))
        return 1;
      return 0;
}

int main( int argc, char** argv ) {


    if( argc != 2 ) {
        show_help( *argv );
        return 0;
    }
    int err =0;
    char *htmlpage, *abs_url;
    char urlspace[MAXURL];
    docid_t docid;
    
    strncpy( urlspace, argv[1], MAXURL );
    docid_sanitizeUrl( urlspace, MAXURL );
    docid =docid_make( urlspace, strlen( urlspace ) );

    // Open mylinks.txt
    FILE *fp;
    fp = fopen( "mylinks.txt", "w" );
   
    // Pre-alloc the queue and set the separator char
    queue_t q;
    queue_create( &q, MAXQSIZE, '\n' );
    queue_push( &q, urlspace, strlen( urlspace ), docid ); // initial url

    
    //  The loop limitation, MAXDOWNLOADS is the maximum number of downloads we
    //  will allow the robot to perform.  It is just a precaution for this assignment
    //  to minimize runaway bots
        
    for( int k=0; k < MAXDOWNLOADS; k++ )
    {
        printf("\nDownload #: %d   Weblinks: %d   Queue Size: %d\n", k+1, q.count, queue_bytesInUse( &q ) );

        // Get the next url from the front of the queue
        if( queue_getCurrent( &q, urlspace, MAXURL ) == 0 ) {
            fprintf( stderr, "No more urls in queue... exiting\n" );
            break;
        }
        queue_pop( &q ); // Pop the current url

        if( isValidDomain( urlspace ) == 0 ) {
            fprintf( stderr, "Alas, '%s' is not within the allowed domain... skipping.\n", urlspace );
            continue;
        }

        fprintf( stderr, "Retrieving '%s'\n", urlspace );

        size_t length =get_webpage( &htmlpage, &abs_url, urlspace );

        if( !length ) { // Some error occured
            fprintf( stderr, "Got error while obtaining '%s'\n", abs_url );
            continue;
        }

        size_t abs_url_len =docid_sanitizeUrl( abs_url, MAXURL );
        docid =docid_make( abs_url, abs_url_len );
        fprintf( stderr, "Got %u bytes from '%s' (DOCID 0x%x)\n", length, abs_url, docid );

                
        if( ( err =index_appendRepository( docid, abs_url, abs_url_len, htmlpage, length ) ) != 0 ) {
            fprintf( stderr, "ERROR: index_appendRepository() returned %d\n", err );
            return -1;
        }
        
        if( ( err =index_appendWebidx( docid, abs_url, abs_url_len ) ) != 0 ) {
            fprintf( stderr, "ERROR: index_appendWebidx() returned %d\n", err );
            return -1;
        }

        if( ( err =parse_webpage( docid, htmlpage, length, abs_url, &q ) ) < 0 ) {
            fprintf( stderr, "ERROR: parse_webpage() returned %d\n", err );
            return -1;
        }

        // Write this link to the mylinks.txt
        fprintf( fp, "%s\n", abs_url );

        /* fprintf( stdout, "%s", links );
    
        if( !get_image_links_from_webpage( &imgs, htmlpage, length, abs_url ) ) {
            return -1; // Error
        }

        fprintf( stdout, "%s", imgs );*/
        
        free( htmlpage );
        free( abs_url );
    }

    // Write the remainder of the queue to mylinks.txt
    while( q.count ) {
        queue_getCurrent( &q, urlspace, MAXURL );
        fprintf( fp, "%s\n", urlspace );
        queue_pop( &q );
    }

    queue_free( &q );

    return 0;
}
