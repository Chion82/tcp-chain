#include <ev.h>
#include <netinet/in.h>

#include "tcp_chain.h"

#define MAX_PLUGINS 32
#define MAX_RELAYS 16384

struct relay_wrap {
  struct ev_io io;
  struct relay_info *relay;
};

struct relay_info {
  int active;
  int sock_fd;
  void *shared_data;
  struct sockaddr_in src_addr;
  struct sockaddr_in dst_addr;
  int takeovered;
  struct relay_wrap read_io_wrap;
  struct relay_wrap write_io_wrap;
  struct sock_info plugin_socks[MAX_PLUGINS];
  char* pending_send_data;
  size_t pending_send_data_len;
  size_t pending_send_data_buf_len;
};

struct plugin_hooks {
  void (*on_connect)();
  void (*on_send)();
  void (*on_recv)();
  void (*on_close)();
  void (*on_init)();
  void (*pause_remote_recv)();
};

void load_plugins();
int init_server_socket();

int init_relay(int sock_fd, struct sockaddr* src_addr, struct sockaddr* dst_addr);
void close_relay(int relay_id);

void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void write_cb(struct ev_loop *loop, struct ev_io *w_, int revents);
