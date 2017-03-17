/*
 * Websearch - webquery_main.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "docid.h"
#include "ranklist.h"
#include "index.h"

#define MAX_KWSIZE 2048
#define MAX_TITLESIZE 80
#define MAX_URLSIZE 1024
#define MAX_PREVIEWSIZE 512
#define DOCID_STRLEN 16

#define IMGCOMPARE_PATH "../imgcompare/imgcompare"
#define IMGCOMPARE_LIMIT 25

typedef enum { MODE_WEB, MODE_IMAGES, MODE_COLOR } mode_t;

void
make_ranklist( ranklist_t *r, const char* keyword, mode_t mode ) {
    index_t from_idx =IDX_WEBIDX, to_idx =IDX_TITLEIDX;
    if( mode == MODE_IMAGES ) {
        from_idx =IDX_IMAGEIDX;
        to_idx =IDX_IMAGEIDX;
    }

    for( index_t idx =from_idx; idx <= to_idx; idx++ ) {
        FILE* file =index_open( (index_t)idx, keyword, IDX_OPEN_READ );
        if( file == NULL ) continue;

        docid_t docid;

        while( !feof( file ) ) {
            if( fread( &docid, sizeof( docid_t ), 1, file ) != 1 )
                break;
            ranklist_push( r, docid, (index_t)idx );
        }
        fclose( file );
    }

    ranklist_sort( r );
}

FILE*
make_imgcompare( const char* source ) {
    char cmd[1024];
    sprintf( cmd, "%s images/%s images/ imgcompare.txt", IMGCOMPARE_PATH, source );
    if( system( cmd ) != 0 )
        return NULL;
    return fopen( "imgcompare.txt", "r" );
}

int main( int argc, char** argv ) {
    int err =0;
    char keyword[MAX_KWSIZE];
    FILE *file =fopen( "queryterms.txt", "r" );
    if( file == NULL ) return -1;
    fscanf( file, "%2048s", keyword );
    fclose( file );

    mode_t mode =MODE_WEB;

    if( argc > 1 ) {
        if( strcmp( argv[1], "--images" ) == 0 ) 
            mode =MODE_IMAGES;
        else if( strcmp( argv[1], "--color" ) == 0 ) 
            mode =MODE_COLOR;
    }

    FILE* imgcompare_out;
    ranklist_t r;
    ranklist_create( &r );
    
    char title[MAX_TITLESIZE];
    char url[MAX_URLSIZE];
    char preview[MAX_PREVIEWSIZE];

    switch( mode ) {
        case MODE_WEB:
        case MODE_IMAGES:
            make_ranklist( &r, keyword, mode );

            printf( "<h2>Results for `%s'</h2>", keyword );
            break;
        case MODE_COLOR:
            imgcompare_out =make_imgcompare( keyword );
            if( imgcompare_out == NULL ) {
                fprintf( stderr, "imgcompare returned with errors\n" );
                err = -1;
            }
            FILE* file =index_open( IDX_REPOSITORY, keyword, IDX_OPEN_READ );
            if( file == NULL ) break;
            if( fgets( url, MAX_URLSIZE-1, file ) == NULL ) break;
            fclose( file);
            
            printf( "<h2>Images similar to:</h2>" );
            printf( "<div class=\"img-query\">\n" );
            printf( "\t<a href=\"%s\"><img src=\"%s\"/></a>\n", url, url );
            printf( "</div>" );

            break;
    }

    int i =0;

    while( 1 ) {
        char *docid_str =NULL;
        if( mode == MODE_COLOR ) {
            if( i == IMGCOMPARE_LIMIT ) break;
            if( imgcompare_out == NULL || ferror( imgcompare_out) || feof( imgcompare_out ) ) break;
        
            docid_str =malloc( sizeof(char) * (DOCID_STRLEN+2) );
            fgets( docid_str, DOCID_STRLEN+1, imgcompare_out );
        } else {
            if( i == r.count ) break;

            docid_t docid =r.first[i].docid;
            docid_str =docid_tostr( docid );
        }
        i++;

        FILE* file =index_open( IDX_REPOSITORY, docid_str, IDX_OPEN_READ );
        if( file == NULL ) continue;
        if( fgets( url, MAX_URLSIZE-1, file ) == NULL ) continue;

        if( mode == MODE_WEB ) {
            if( fgets( title, MAX_TITLESIZE-1, file ) == NULL ) continue;
            size_t preview_size =0;
            preview_size = fread( preview, sizeof(char), MAX_PREVIEWSIZE-1, file );
            preview[preview_size] =0;

            
            printf( "<div class=\"result\">\n" );
            printf( "\t<h3><a href=\"%s\">%s</a></h3>\n", url, title );
            printf( "\t<cite>%s</cite>\n", url );
            printf( "\t<p>%s...</p>\n", preview );
            printf( "</div>\n" );
        } else {
            printf( "<div class=\"img-result\">\n" );
            printf( "\t<a href=\"%s\"><img src=\"%s\"/></a>\n", url, url );
            if( mode == MODE_IMAGES )
                printf( "\t<a class=\"color-link\" href=\"?color&q=%s\">find by color</a>", docid_str );
            printf( "</div>" );
        }

        free( docid_str );
        fclose( file );
    }

    ranklist_free( &r );
    
    if( !i )
        printf( "<p>Your query returned no results</p>" );
    return 0;

}
