#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "libuv/include/uv.h"
#include "http-parser/http_parser.h"


#define RESPONSE \
  "HTTP/1.1 200 OK\r\n" \
  "Content-Type: text/plain\r\n" \
  "Content-Length: 12\r\n" \
  "\r\n" \
  "hello world\n"

static uv_buf_t resbuf;
static uv_tcp_t server;
static http_parser_settings settings;

typedef struct {
  uv_tcp_t tcp;
  http_parser parser;
  uv_write_t write_req;
} client_t;


void on_close(uv_handle_t* handle) {
  free(handle);
}


static uv_buf_t on_alloc(uv_handle_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = malloc(suggested_size);
  buf.len = suggested_size;
  return buf;
}


static void on_read(uv_stream_t* handle, int nread, uv_buf_t buf) {
  client_t* client = handle->data;
  size_t parsed;

  if (nread >= 0) {
    /* parse http */

    parsed = http_parser_execute(&client->parser,
                                 &settings,
                                 buf.base,
                                 nread);

    if (parsed < nread) {
      printf("parse error");
      uv_close((uv_handle_t*)handle, on_close);
    }

  } else { 
    uv_err_t err = uv_last_error(handle->loop);

    if (err.code == UV_EOF) {
      /* do nothing */
    } else {
      fprintf(stderr, "read: %s\n", uv_strerror(err));
    }

    uv_close((uv_handle_t*)handle, on_close);
  }

  free(buf.base);
}


void on_connected(uv_stream_t* s, int status) {
  assert(s == (uv_stream_t*)&server);
  assert(status == 0);


  client_t* client = malloc(sizeof(client_t));


  int r = uv_tcp_init(s->loop, &client->tcp);
  r = uv_accept((uv_stream_t*)&server, (uv_stream_t*)&client->tcp);

  if (r) { 
    uv_err_t err = uv_last_error(s->loop);
    fprintf(stderr, "accept: %s\n", uv_strerror(err));
    return;
  }

  client->tcp.data = client;
  client->parser.data = client;

  http_parser_init(&client->parser, HTTP_REQUEST);

  uv_read_start((uv_stream_t*)&client->tcp, on_alloc, on_read);
}

static void write_cb(uv_write_t* req, int status) {
  uv_close((uv_handle_t*)req->data, on_close);
}


int on_headers_complete(http_parser* parser) {
  client_t* client = parser->data;

  client->write_req.data = client;
  uv_write(&client->write_req, (uv_stream_t*)&client->tcp, &resbuf, 1, write_cb);

  return 1;
}



int main() {

  resbuf.base = RESPONSE;
  resbuf.len = sizeof(RESPONSE);

  settings.on_headers_complete = on_headers_complete;

  uv_tcp_init(uv_default_loop(), &server);

  int r = uv_tcp_bind(&server, uv_ip4_addr("0.0.0.0", 8000));

  if (r) { 
    uv_err_t err = uv_last_error(server.loop);
    fprintf(stderr, "bind: %s\n", uv_strerror(err));
    return -1;
  }

  r = uv_listen((uv_stream_t*)&server, 128, on_connected);

  if (r) { 
    uv_err_t err = uv_last_error(server.loop);
    fprintf(stderr, "listen: %s\n", uv_strerror(err));
    return -1;
  }

  uv_run(server.loop);

  return 0;
}
