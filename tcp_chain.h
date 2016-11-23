#include <netinet/in.h>
#include <ev.h>

#define BUFFER_SIZE 2048

struct sock_info {
  int relay_id;
  int plugin_id;
  int* takeovered;
  void* data;
  void* shared_data;
  struct sockaddr* src_addr;
  struct sockaddr* dst_addr;
};

struct init_info {
  struct ev_loop* default_loop;
  int plugin_id;
  int (*relay_send)();
  int (*relay_close)();
};

int relay_send_func(struct sock_info* identifier, const void *buffer, size_t length, int flags);
int relay_close_func(struct sock_info* identifier);

void on_connect(struct sock_info* identifier);
void on_recv(struct sock_info* identifier, char* data, size_t* length);
void on_send(struct sock_info* identifier, char* data, size_t* length);
void on_close(struct sock_info* identifier);
void on_init(struct init_info* info);
