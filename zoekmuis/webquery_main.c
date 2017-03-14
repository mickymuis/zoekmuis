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

int main( int argc, char** argv ) {
    char keyword[MAX_KWSIZE];
    FILE *file =fopen( "queryterms.txt", "r" );
    if( file == NULL ) return -1;
    fscanf( file, "%2048s", keyword );
    fclose( file );

    enum { MODE_WEB, MODE_IMAGES, MODE_COLOR } mode;
    mode =MODE_WEB;

    if( argc > 1 ) {
        if( strcmp( argv[1], "--images" ) == 0 ) 
            mode =MODE_IMAGES;
        else if( strcmp( argv[1], "--color" ) == 0 ) 
            mode =MODE_COLOR;
    }

    ranklist_t r;
    ranklist_create( &r );

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
            ranklist_push( &r, docid, (index_t)idx );
        }
    }

    ranklist_sort( &r );

    char title[MAX_TITLESIZE];
    char url[MAX_URLSIZE];
    char preview[MAX_PREVIEWSIZE];
//    int image_cell =0;

    for( int i =0; i < r.count; i++ ) {
        //printf( "%#Lx ranking #%d\n", r.first[i].docid, r.first[i].rank );
        docid_t docid =r.first[i].docid;
        char* docid_str =docid_tostr( docid );
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
        } else if( mode == MODE_IMAGES ) {
            printf( "<div class=\"img-result\">\n" );
            printf( "\t<a href=\"%s\"><img src=\"%s\"/></a>\n", url, url );
            printf( "\t<a class=\"color-link\" href=\"?color&docid=%s\">find by color</a>", docid_str );
            printf( "</div>" );
        }

        free( docid_str );
        fclose( file );
    }

    ranklist_free( &r );
    return 0;

}
