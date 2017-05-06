#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <linux/netfilter_ipv4.h>
#include <ev.h>
#include <errno.h>
#include <fcntl.h>

#include "../tcp_chain.h"
#include "../lib/hiredis/hiredis.h"

//In each period of MONITOR_TIME, connection requests from the same  
//ip address can mostly be recieved as mamy as MAX_REQUEST, or this
//ip address will be supposed to be banned for the duration of BAN_TIME.
#define MONITOR_TIME "60"
#define MAX_REQUEST 5
#define BAN_TIME "60"

#define MAX_CMD_LEN 128

//GLOBAL CONFIGUARTION
#define REDIS_SERVER_ADDR "127.0.0.1"
#define REDIS_SERVER_PORT 6379


int (*relay_send)();
int (*relay_close)();
void (*relay_pause_recv)();


//form all args into one single command
void command(char* cmd, char* arg1, ...){   
  va_list arg_ptr;
  va_start(arg_ptr,arg1);
  char temp[MAX_CMD_LEN];
  strcpy(cmd,arg1);
  strcpy(temp,va_arg(arg_ptr,char*));
  
  while(strcmp(temp,"0") != 0){
    strcat(cmd," ");
  	strcat(cmd,temp);
  	strcpy(temp,va_arg(arg_ptr,char*));
  }
  
  va_end(arg_ptr);
}

void process_error(redisContext* c, redisReply* r, struct sock_info* identifier){
  if(r != NULL){
  	freeReplyObject(r);
  }
  redisFree(c);
  (*relay_close)(identifier);
  LOG("[ANTI-BRUTE-FORCE] Error Occurred.");
}



void on_init(struct init_info* info) {
  relay_send = info->relay_send;
  relay_close = info->relay_close;
  relay_pause_recv = info->relay_pause_recv;
}



void on_connect(struct sock_info* identifier) {
  char* addr = inet_ntoa(((struct sockaddr_in*)(identifier->src_addr))->sin_addr);
  //LOG("[ANTI-BRUTE_FORCE]: Request From:%s",addr);
  
  redisReply* r;
  char cmd[MAX_CMD_LEN];
  redisContext* c = redisConnect(REDIS_SERVER_ADDR,REDIS_SERVER_PORT);
  if(c->err){ 
    LOG("[ANTI-BRUTE-FORCE] Fatal Error : Cannot Connect to Redis Server.");
	goto error; 
  }
  command(cmd,"GET",addr,"0");
  r = (redisReply*)redisCommand(c,cmd);
  
  
  //key does not exist
  if(r->type == REDIS_REPLY_NIL){
  freeReplyObject(r);
  command(cmd,"SETEX",addr,MONITOR_TIME,"1","0");
  r = (redisReply*)redisCommand(c,cmd);
  if(!(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK") == 0)){ goto error;}
  }

  //key exists
  else if(r->type == REDIS_REPLY_STRING){
    int count = atoi(r->str);
    //numebr of requests has reached the limit
    if(count >= MAX_REQUEST){
      freeReplyObject(r);
      command(cmd,"SET",addr,"-1","0");
      r = (redisReply*)redisCommand(c,cmd);
      if(!(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK") == 0)){ goto error;}
      freeReplyObject(r);
      command(cmd,"EXPIRE",addr,BAN_TIME,"0");
      r = (redisReply*)redisCommand(c,cmd);
      if(!(r->type == REDIS_REPLY_INTEGER && r->integer == 1)){ goto error;}
      LOG("[ANTI-BRUTE-FORCE] IP Banned Temporarily: %s",addr);
      (*relay_close)(identifier);
   }
    //IP has been banned
    else if(count == -1){      
      (*relay_close)(identifier);
    }
    //normal situations
    else{
      freeReplyObject(r);
      command(cmd,"INCR",addr,"0");
      r = (redisReply*)redisCommand(c,cmd);
      if(r->type != REDIS_REPLY_INTEGER){ goto error;}
    }
  }
  freeReplyObject(r);
  redisFree(c);
  return;

error: 
  LOG("[ANTI-BRUTE-FORCE] Failed to Execute Command: %s",cmd);
  process_error(c,r,identifier);
  return;
}



void on_send(struct sock_info* identifier, char** p_data, size_t* length) {
  //printf("on_send() invoked.\n");
}



void on_recv(struct sock_info* identifier, char** p_data, size_t* length) {
  //printf("on_recv() invoked.\n");
}



void on_close(struct sock_info* identifier) {
  //printf("on_close() invoked.\n");
}






