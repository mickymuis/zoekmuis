all: webspider webquery

webspider: webspider_main.c webspider.c queue.c docid.c index.c ranklist.c htmlstreamparser.c hash.c avl.c
	gcc -std=c99 -g webspider_main.c webspider.c queue.c docid.c index.c ranklist.c htmlstreamparser.c hash.c avl.c -o webspider -lcurl

webquery: webquery_main.c docid.c index.c ranklist.c hash.c avl.c
	gcc -std=c99 -g webquery_main.c docid.c index.c ranklist.c hash.c avl.c -o webquery

clean:
	rm -f webspider webquery
	rm -f titleindex/*
	rm -f webindex/*
	rm -f linkindex/*
	rm -f pageindex/*
	rm -f repository/*
