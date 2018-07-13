httpd: httpd.c threadpool.c condition.c
	gcc -o $@ $^ -lpthread

.PHONY: clean

clean:
	rm -f httpd
