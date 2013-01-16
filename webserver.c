#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "uv/uv.h"
#include "http_parser/http_parser.h"


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
  uv_tcp_t handle;
  http_parser parser;
  uv_req_t write_req;
} client_t;


void on_close(uv_handle_t* handle) {
  free(handle);
}


uv_buf_t on_alloc(uv_tcp_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = malloc(suggested_size);
  buf.len = suggested_size;
  return buf;
}


void on_read(uv_tcp_t* handle, int nread, uv_buf_t buf) {
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
    uv_err_t err = uv_last_error();

    if (err.code == UV_EOF) {
      /* do nothing */
    } else {
      fprintf(stderr, "read: %s\n", uv_strerror(err));
    }

    uv_close((uv_handle_t*)handle, on_close);
  }

  free(buf.base);
}


void on_connected(uv_tcp_t* s, int status) {
  assert(s == &server);
  assert(status == 0);


  client_t* client = malloc(sizeof(client_t));

  int r = uv_accept(&server, &client->handle);

  if (r) { 
    uv_err_t err = uv_last_error();
    fprintf(stderr, "accept: %s\n", uv_strerror(err));
    return;
  }

  client->handle.data = client;
  client->parser.data = client;

  http_parser_init(&client->parser, HTTP_REQUEST);

  uv_read_start(&client->handle, on_alloc, on_read);
}

void after_write(uv_req_t* req, int status) {
  uv_close(req->handle, on_close);
}


int on_headers_complete(http_parser* parser) {
  client_t* client = parser->data;

  uv_tcp_t* handle = &client->handle;

  uv_req_init(&client->write_req, (uv_handle_t*)handle, after_write);

  uv_write(&client->write_req, &resbuf, 1);

  return 1;
}



int main() {
  uv_init();

  resbuf.base = RESPONSE;
  resbuf.len = sizeof(RESPONSE);

  settings.on_headers_complete = on_headers_complete;

  uv_tcp_init(&server);

  int r = uv_bind(&server, uv_ip4_addr("0.0.0.0", 8000));

  if (r) { 
    uv_err_t err = uv_last_error();
    fprintf(stderr, "bind: %s\n", uv_strerror(err));
    return -1;
  }

  r = uv_listen(&server, 128, on_connected);

  if (r) { 
    uv_err_t err = uv_last_error();
    fprintf(stderr, "listen: %s\n", uv_strerror(err));
    return -1;
  }

  uv_run();

  return 0;
}
