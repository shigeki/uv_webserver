webserver: webserver.c uv/uv.a http_parser/http_parser.o
	gcc -o webserver webserver.c uv/uv.a http_parser/http_parser.o

uv/uv.a:
	make -C uv

http_parser/http_parser.o:
	make -C http_parser http_parser.o

clean:
	make -C http_parser clean
	make -C uv distclean
	rm webserver
