#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <linux/netfilter_ipv4.h>
#include <ev.h>

#include "../tcp_chain.h"

struct proxy_wrap {
  ev_io io;
  struct sock_info* identifier;
  int (*relay_send)();
  int (*relay_close)();
};

struct ev_loop* default_loop;

void remote_read_cb(struct ev_loop *loop, struct ev_io *w_, int revents) {
  char buffer[BUFFER_SIZE];

  if(EV_ERROR & revents) {
    return;
  }

  struct proxy_wrap* proxy = (struct proxy_wrap*)w_;
  struct ev_io* io = &(proxy->io);

  int remote_fd = io->fd;
  ssize_t buf_len = recv(remote_fd, buffer, BUFFER_SIZE, 0);

  if (buf_len <= 0) {
    printf("Remote closed connection.\n");
    (*(proxy->relay_close))(proxy->identifier);
    return;
  }

  (*(proxy->relay_send))(proxy->identifier, buffer, buf_len, 0);

}

void on_connect(struct sock_info* identifier, int (*relay_send)(), int (*relay_close)()) {
  //printf("on_connect() invoked.\n");
  printf("relay_id: %d\n", identifier->relay_id);

  int remote_sock = socket(AF_INET, SOCK_STREAM, 0);

  int sock_mark = 100;
  setsockopt(remote_sock, SOL_SOCKET, SO_MARK, &sock_mark, sizeof(sock_mark));

  int keep_alive = 1;
  int keep_idle = 40;
  int keep_interval = 5;
  int keep_count = 3;
  setsockopt(remote_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keep_alive, sizeof(keep_alive));
  setsockopt(remote_sock, SOL_TCP, TCP_KEEPIDLE, (void*)&keep_idle, sizeof(keep_idle));
  setsockopt(remote_sock, SOL_TCP, TCP_KEEPINTVL, (void *)&keep_interval, sizeof(keep_interval)); 
  setsockopt(remote_sock, SOL_TCP, TCP_KEEPCNT, (void *)&keep_count, sizeof(keep_count));


  if (connect(remote_sock, identifier->dst_addr, sizeof(struct sockaddr)) < 0) {
    printf("Remote connection failed.\n");
    (*relay_close)(identifier);
    return;
  }

  printf("Remote connected.\n");

  struct proxy_wrap* proxy = (struct proxy_wrap*)malloc(sizeof(struct proxy_wrap));
  proxy->identifier = identifier;
  proxy->relay_send = relay_send;
  proxy->relay_close = relay_close;

  identifier->data = (void*)proxy;
  *(identifier->takeovered) = 1;

  ev_io_init(&(proxy->io), remote_read_cb, remote_sock, EV_READ);
  ev_io_start(default_loop, &(proxy->io));
}

void on_recv(struct sock_info* identifier, char* data, size_t* length, int (*relay_send)(), int (*relay_close)()) {
  //printf("on_recv() invoked.\n");
  struct proxy_wrap* proxy = (struct proxy_wrap*)(identifier->data);
  send((proxy->io).fd, data, *length, 0);
}

void on_send(struct sock_info* identifier, char* data, size_t* length, int (*relay_send)(), int (*relay_close)()) {
  //printf("on_send() invoked.\n");
}

void on_close(struct sock_info* identifier) {
  //printf("on_close() invoked.\n");
  struct proxy_wrap* proxy = (struct proxy_wrap*)(identifier->data);
  if (proxy != NULL) {
    ev_io_stop(default_loop, &(proxy->io));
    close((proxy->io).fd);
    free(identifier->data);
  }
}

void on_init(struct init_info* info) {
  default_loop = info->default_loop;
}
