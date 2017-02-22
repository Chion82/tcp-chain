#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "../tcp_chain.h"

void on_init(struct init_info* info) {
  LOG("Logger plugin loaded. Plugin ID is %d.", info->plugin_id);
}

void on_connect(struct sock_info* identifier) {
  struct sockaddr_in* src_addr = (struct sockaddr_in*)(identifier->src_addr);
  struct sockaddr_in* dst_addr = (struct sockaddr_in*)(identifier->dst_addr);
  char src_ip[128], dst_ip[128], *p_temp;
  p_temp = inet_ntoa(src_addr->sin_addr);
  strcpy(src_ip, p_temp);
  p_temp = inet_ntoa(dst_addr->sin_addr);
  strcpy(dst_ip, p_temp);
  LOG("New connection from %s:%d to %s:%d, relay ID is %d.", 
    src_ip, ntohs(src_addr->sin_port), 
    dst_ip, ntohs(dst_addr->sin_port), 
    identifier->relay_id);
}

void on_recv(struct sock_info* identifier, char** p_data, size_t* length) {
  // LOG("Connection %d received %d bytes.", identifier->relay_id, *length);

  // Example: modify buffer data
  // struct sockaddr_in* dst_addr = (struct sockaddr_in*)(identifier->dst_addr);
  // if (ntohs(dst_addr->sin_port) == 5001) {
  //   LOG("Modifying recv buffer.");
  //   *p_data = realloc(*p_data, *length + 16);
  //   memcpy(*p_data + *length, "recvrecvrecvrecv", 16);
  //   *length += 16;
  // }
}

void on_send(struct sock_info* identifier, char** p_data, size_t* length) {
  // LOG("Connection %d sent %d bytes", identifier->relay_id, *length);

  // Example: modify buffer data
  // struct sockaddr_in* dst_addr = (struct sockaddr_in*)(identifier->dst_addr);
  // if (ntohs(dst_addr->sin_port) == 5001) {
  //   LOG("Modifying send buffer.");
  //   *p_data = realloc(*p_data, *length + 16);
  //   memcpy(*p_data + *length, "sendsendsendsend", 16);
  //   *length += 16;
  // }
}

void on_close(struct sock_info* identifier) {
  LOG("Connection %d closing.", identifier->relay_id);
}
