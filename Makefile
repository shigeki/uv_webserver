webserver: webserver.c libuv/libuv.a http-parser/http_parser.o
	gcc -o webserver webserver.c libuv/libuv.a http-parser/http_parser.o -lpthread -lrt

libuv/libuv.a:
	make -C libuv

http_parser/http_parser.o:
	make -C http-parser http_parser.o

clean:
	make -C http-parser clean
	make -C libuv distclean
	rm webserver
