#include <netinet/in.h>
#include <ev.h>
#include <stdarg.h>
#include <time.h>

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
  void (*relay_pause_recv)();
  int argc;
  char** argv;
};

int relay_send_func(struct sock_info* identifier, char *buffer, size_t length, int flags);
int relay_close_func(struct sock_info* identifier);
void relay_pause_recv_func(struct sock_info* identifier, int pause);

void on_init(struct init_info* info);
void on_connect(struct sock_info* identifier);
void on_recv(struct sock_info* identifier, char** p_data, size_t* length);
void on_send(struct sock_info* identifier, char** p_data, size_t* length);
void on_close(struct sock_info* identifier);
void pause_remote_recv(struct sock_info* identifier, int pause);

void LOG(const char* message, ...) {
  time_t now = time(NULL);
  char timestr[20];
  strftime(timestr, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
  printf("[%s] ", timestr);
  va_list argptr;
  va_start(argptr, message);
  vfprintf(stdout, message, argptr);
  va_end(argptr);
  printf("\n");
  fflush(stdout);
}
