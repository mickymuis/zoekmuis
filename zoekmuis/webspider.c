/*
 * Websearch - webspider.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 */
#include "webspider.h"
#include <stdio.h> 
#include <string.h>
#include <curl/curl.h>
#include "htmlstreamparser.h"
#include "index.h"

#define TAG_IMG "img"
#define TAG_IMG_LEN 3
#define TAG_A "a"
#define TAG_A_LEN 1
#define TAG_ENDTITLE "/title"
#define TAG_ENDTITLE_LEN 6
#define ATTR_HREF "href"
#define ATTR_HREF_LEN 4
#define ATTR_SRC "src"
#define ATTR_SRC_LEN 3

static size_t write_callback( char *buffer, size_t size, size_t nmemb, void *userp );

typedef struct {
    char *data;
    size_t size;
} dataptr_t;

static void
dataptr_init( dataptr_t* d ) {
    d->data =malloc( 1 );
    d->size =0;
}

static void
dataptr_free( dataptr_t* d ) {
    free( d->data );
    d->size =0;
}

static int
dataptr_grow( dataptr_t* d, size_t add ) {
    d->data =realloc( d->data, d->size + add );
    d->size +=add;
    return (d->data == NULL );
}

size_t
make_absolute( char **buffer, const char* link, size_t length, const char* abs_url ) {
    int make_abs =0;
    char *data =NULL;
    size_t entrylength =0, abs_len =0;

    // Check for exclusions, TODO: add more exclusions
    if( strncmp( link, "mailto:", 7 ) == 0 )
        return 0;

    // Check for relative links and add the prefix is neccesary
    if( strncmp( link, "http", 4 ) != 0 ) {
        make_abs =1;
        abs_len= strlen( abs_url );
        if( abs_url[abs_len-1] == '/' && link[0] == '/' )
            abs_len--; // Avoid double '/'
        entrylength +=abs_len;

    }
    entrylength +=length;
    
    data =(char*)malloc( entrylength+1 * sizeof(char) );
    if( data == NULL )
        return 0;

    if( make_abs )
        memcpy( data, abs_url, abs_len );
    memcpy( data + abs_len, link, length );

    data[entrylength] =0; // Not neccesary, but handy for debugging
    *buffer =data;

    return entrylength;
}

size_t
get_webpage( char** buffer, char** effective_url, const char* url ) {
	
        dataptr_t dataptr;
        dataptr_init( &dataptr );
  
        CURL *curl = curl_easy_init( );

	/* tell curl the URL address we are going to download */
	curl_easy_setopt( curl, CURLOPT_URL, url );
	
	/* Pass a pointer to the function write_callback( char *ptr, size_t size, size_t nmemb, void *userdata); 
           write_callback gets called by libcurl as soon as there is data received, 
           and we can process the received data, such as saving and weblinks extraction. */
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_callback );

        /* Pass a pointer to the dataptr_t structure to the write callback */
        curl_easy_setopt( curl, CURLOPT_WRITEDATA, (void*)&dataptr );

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L ); // ten second timeout
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "showlinks/1.0"); 

	/* Tell curl to perform the action */
	CURLcode curl_res = curl_easy_perform( curl );

	if( curl_res ) {
		fprintf( stderr, "ERROR: while dowloading file: incorrect url or timeout.\n" ); 
        }

        // Store the effective url (after possible redirect)

        char* buf;
        curl_res = curl_easy_getinfo( curl, CURLINFO_EFFECTIVE_URL, &buf ); 
        *effective_url =(char*)malloc( (strlen( buf ) + 1) * sizeof( char ) );
        strcpy( *effective_url, buf );
	
        // Cleanup
	curl_easy_cleanup(curl);
    

        if( dataptr.size ==0 )
            dataptr_free( &dataptr );
        else {
            // Add trailing \0
            dataptr_grow( &dataptr, 1 );
            dataptr.data[dataptr.size-1] =0;
            *buffer =dataptr.data;
        }

        return dataptr.size; // Will be 0 on failure
}

size_t
parse_webpage( docid_t base_docid, char* htmlpage, size_t length, const char* abs_url, queue_t* q ) {

	/*a pointer to the HTMLSTREAMPARSER structure and initialization*/
	HTMLSTREAMPARSER *hsp = html_parser_init( ); 
	char tag_buf[9];
	char attr_buf[9];
	char val_buf[128]; 	
        char inner_buf[8192];
	html_parser_set_tag_to_lower(hsp, 1);   
	html_parser_set_attr_to_lower(hsp, 1); 
	html_parser_set_tag_buffer(hsp, tag_buf, sizeof(tag_buf));  
	html_parser_set_attr_buffer(hsp, attr_buf, sizeof(attr_buf));    
	html_parser_set_val_buffer(hsp, val_buf, sizeof(val_buf)-1); 
        html_parser_set_inner_text_buffer(hsp, inner_buf, sizeof(inner_buf)-1);

        dataptr_t dataptr;
        dataptr_init( &dataptr );
        int count =0;

	for ( int i = 0; i < length; i++) 
	{             
		html_parser_char_parse(hsp, ((char *)htmlpage)[i]);

                /* Case 1: found `<a href=', add link to queue and link-index */
		if (html_parser_cmp_tag( hsp, TAG_A, TAG_A_LEN )) {
			if (html_parser_cmp_attr( hsp, ATTR_HREF, ATTR_HREF_LEN ))  
				if (html_parser_is_in(hsp, HTML_VALUE_ENDED))
				{ 
                                    char *buffer;
                                    size_t len =make_absolute( &buffer, html_parser_val( hsp ), html_parser_val_length( hsp ), abs_url );
                                    len =docid_sanitizeUrl( buffer, len );
                                    
                                    if( !len ) continue;
                                    docid_t docid =docid_make( buffer, len );

                                    int err;
                                    if( err = queue_push( q, buffer, len, docid )
                                            != 0 ) {
                                        free( buffer );
                                        return err;
                                    }

                                    index_appendLinkidx( docid, base_docid );
                                    //fprintf( stderr, "--- Website HREF: %s\n", buffer );
                                    free( buffer );
                                    count++;
				}
                }
                // Case 2: (end of) the <title> tag
                else if( html_parser_cmp_tag( hsp, TAG_ENDTITLE, TAG_ENDTITLE_LEN ) ) {
		    if (html_parser_is_in(hsp, HTML_TAG_END)){

                        size_t title_len = html_parser_inner_text_length(hsp);
                        char* title = html_parser_replace_spaces(html_parser_trim(html_parser_inner_text(hsp), &title_len), &title_len);
                        
                        if( !title_len ) continue;
                        title[title_len] =0;
                        fprintf( stderr, "--- Website Title: %s\n", title );

                        index_appendHtmlInner( IDX_TITLEIDX, base_docid, title );
                       // html_parser_release_inner_text_buffer(hsp);
                    }
                }
                // Case 3: inner text any other element
                else if (html_parser_is_in(hsp, HTML_TAG_END) && html_parser_is_in(hsp, HTML_CLOSING_TAG) ) {
                        size_t text_len = html_parser_inner_text_length(hsp);
                        char* text = html_parser_replace_spaces(html_parser_trim(html_parser_inner_text(hsp), &text_len), &text_len);
                        
                        if( !text_len ) continue;
                        text[text_len] =0;
//                        fprintf( stderr, "--- Inner text portion: `%s'\n", text );
                    
                        index_appendHtmlInner( IDX_PAGEIDX, base_docid, text );
                }
                      
	}		
        
	// release the hsp
	html_parser_cleanup(hsp);
         

        /*if( dataptr.size ==0 )
            dataptr_free( &dataptr );
        else {
            // Add trailing \0
            dataptr_grow( &dataptr, 1 );
            dataptr.data[dataptr.size-1] =0;
            *buffer =dataptr.data;
        }

        return dataptr.size; // Will be NULL on failure
        */
        dataptr_free( &dataptr );
        return count;
}

static size_t 
write_callback( char *buffer, size_t size, size_t nmemb, void *userp )
{
    size_t realsize =size * nmemb;
    dataptr_t *dataptr =(dataptr_t*)userp;
    size_t offset =dataptr->size;

    if( dataptr_grow( dataptr, realsize ) ) {
        fprintf( stderr, "ERROR: Some weird memory problem. Panic.\n" );
        return 0;
    }

    memcpy( (char*)dataptr->data + offset, buffer, realsize );

    return realsize;
} 
