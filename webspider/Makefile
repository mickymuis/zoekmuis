all: webspider webquery

webspider: webspider_main.c webspider.c queue.c docid.c index.c htmlstreamparser.c hash.c avl.c
	gcc -std=c99 -g webspider_main.c webspider.c queue.c docid.c index.c htmlstreamparser.c hash.c avl.c -o webspider -lcurl

webquery: webquery_main.c docid.c index.c hash.c
	gcc -std=c99 -g webquery_main.c docid.c index.c hash.c -o webquery

clean:
	rm -f webspider webquery
