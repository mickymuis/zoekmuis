all: webspider webquery

webspider: webspider_main.c webspider.c queue.c docid.c index.c ranklist.c htmlstreamparser.c hash.c avl.c
	gcc -std=c99 -g webspider_main.c webspider.c queue.c docid.c index.c ranklist.c htmlstreamparser.c hash.c avl.c -o webspider -lcurl

webquery: webquery_main.c docid.c index.c ranklist.c hash.c avl.c
	gcc -std=c99 -g webquery_main.c docid.c index.c ranklist.c hash.c avl.c -o webquery
	(cd ../imgcompare/Debug && make)
	[ -e titleindex ] || mkdir titleindex
	[ -e webindex ] || mkdir webindex
	[ -e linkindex ] || mkdir linkindex
	[ -e pageindex ] || mkdir pageindex
	[ -e imageindex ] || mkdir imageindex
	[ -e repository ] || mkdir repository
	[ -e images ] || mkdir images


clean:
	rm -f webspider webquery
	rm -rf titleindex
	rm -rf webindex
	rm -rf linkindex
	rm -rf pageindex
	rm -rf imageindex
	rm -rf repository
	rm -rf images
	(cd ../imgcompare/Debug && make clean)
