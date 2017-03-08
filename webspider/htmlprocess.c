/* This demo aims to: 
1. download the html from the URL based on the libcurl;
2. extract the weblinks information from the URL based on the htmlstreamparser API;
In addition, we are planning to add the function extract other types of multimedia informations
Compile the demo program with this command:
gcc htmlstreamparser.c htmlprocess.c –o htmlprocess –lcurl
Run the program with this command:
./htmlprocess www.liacs.nl
*/

/*Header file part */
#include <stdio.h> 
#include <string.h>
#include <curl/curl.h>
#include "htmlstreamparser.h"

 FILE *tmp1; /*a pointer to the file save the data of HTML file*/
 FILE *tmp2; /*a pointer to the file save web-links from the URL*/
   
/*in this part,HTML file saving and web-links extraction are realised. The web-links stored in HTML are indicated by a herf, so we can compare the string data to get the web-links*/
static size_t write_callback(char *buffer, size_t size, size_t nmemb, HTMLSTREAMPARSER *hsp)
{ 
	/*save the HTML file in tmp1*/
	fwrite(buffer, size, nmemb, (FILE *)tmp1);
	/*the size of the received data*/
	size_t realsize = size * nmemb, p; 

	for (p = 0; p < realsize; p++) 
	{             
		html_parser_char_parse(hsp, ((char *)buffer)[p]);/*Parse the char specified by the char argument*/  
		if (html_parser_cmp_tag(hsp, "a", 1)) /*Compares the tag name and the string “a”.The argument 1 is a string length.Returns 1 for equality otherwise returns 0*/   
			if (html_parser_cmp_attr(hsp, "href", 4))/*Compares the attribute name and the string “href”. The argument 4 is a string length.Returns 1 for equality otherwise returns 0*/  
				if (html_parser_is_in(hsp, HTML_VALUE_ENDED))/*Returns 1 if the parser is inside HTML_VALUE_ENDED part of HTML code.*/
				{ 
					html_parser_val(hsp)[html_parser_val_length(hsp)] = '\0';
					printf("%s\n",html_parser_val(hsp));
					
					/*save the weblinks information in tmp2*/ 
					fwrite(html_parser_val(hsp),html_parser_val_length(hsp),1,(FILE *)tmp2);
					fwrite("\n",1,1,(FILE *)tmp2);
				} 
	}		
	return realsize;
} 

/*initialization curl and call function to save the HTML file*/
static void Download_URL(char * url,HTMLSTREAMPARSER *hsp)
{
	/*curl_easy_init() initializes curl and this call must have a corresponding call to curl_easy_cleanup();*/	 
	CURL *curl = curl_easy_init();

	/*tell curl the URL address we are going to download*/
	curl_easy_setopt(curl, CURLOPT_URL, url);
	
	/*Pass a pointer to the function write_callback( char *ptr, size_t size, size_t nmemb, void *userdata); write_callback gets called by libcurl as soon as there is data received, and we can process the received data, such as saving and weblinks extraction. */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    /*it tells the the pointer userdata argument in the write_callback function the data comes from hsp pointer*/
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, hsp);
    /*A parameter set to 1 tells the library to follow any Location: header that the server sends as part of a HTTP header.This means that the library will re-send the same request on the new location and follow new Location: headers all the way until no more such headers are returned.*/
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    
	/*allow curl to perform the action*/
	CURLcode curl_res = curl_easy_perform(curl);

	if(curl_res==0)
	{ 
		printf("HTML file downloaded success\n"); 
	}
	else 
	{ 
		printf("ERROR in dowloading file\n"); 
		curl_easy_cleanup(curl); 
	}	
	/*release all the previously allocated memory ,and it corresponds to the function curl_easy_init();*/
	curl_easy_cleanup(curl);
}

/*main function*/
int main(int argc, char *argv[])
{   
    /*set the path to save the html file*/
	tmp1=fopen("./htmldata.html", "w");
	/*set the path to save the extracted web-links*/
	tmp2=fopen("./weblinksdata.html", "w");
	/*set the URL address to download*/	
	//char URL[]="http://www.liacs.nl/~mlew"; 

	/*a pointer to the HTMLSTREAMPARSER structure and initialization*/
	HTMLSTREAMPARSER *hsp = html_parser_init( ); 
	char tag[2];
	char attr[5];
	char val[128]; 	
	html_parser_set_tag_to_lower(hsp, 1);   
	html_parser_set_attr_to_lower(hsp, 1); 
	html_parser_set_tag_buffer(hsp, tag, sizeof(tag));  
	html_parser_set_attr_buffer(hsp, attr, sizeof(attr));    
	html_parser_set_val_buffer(hsp, val, sizeof(val)-1); 

	/*run download function*/ 
	Download_URL(argv[1],hsp);

	/*release the hsp*/
	html_parser_cleanup(hsp);

	/*close the file tmp and tmp2 and return*/
	fclose(tmp1);
	fclose(tmp2);
	return EXIT_SUCCESS;
}
