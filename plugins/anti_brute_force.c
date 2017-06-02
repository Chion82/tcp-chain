/*

Command:
--abf-ports    (--abf-p)    :set monitored ports,seperated by ','
--abf-monitor  (--abf-m)    :set monitor time
--abf-request  (--abf-r)    :set max number of requests
--abf-ban      (--abf-b)    :set ban time

*/

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
#include <malloc.h>


#include "../aegis.h"
#include "../lib/hiredis/hiredis.h"

#define PORTS_ADD             10
#define MAX_CMD_LEN           128
#define REDIS_SERVER_ADDR     "127.0.0.1"
#define REDIS_SERVER_PORT     6379
#define DEFAULT_PORTS         "80"            //string,seperated by ','
#define DEFAULT_MONITOR_TIME  "60"
#define DEFAULT_MAX_REQUEST   20
#define DEFAULT_BAN_TIME      "80"
#define MARK_PREFIX           "\033[01;31m"   //set font red
#define MARK_ERROR            "\033[01;35m"   //set font purple
#define MARK_INFO             "\033[01;34m"   //set font blue
#define MARK_SUFFIX           "\033[0m"       //reset font color

int port_num = 0;
int port_max_num = 0;
int* ports;

redisReply* r = NULL;
redisContext* c = NULL;

//In each period of MONITOR_TIME, connection requests from the same  
//ip address can mostly be recieved as mamy as MAX_REQUEST, or this
//ip address will be supposed to be banned for the duration of BAN_TIME.
char* MONITOR_TIME = DEFAULT_MONITOR_TIME;
int   MAX_REQUEST = DEFAULT_MAX_REQUEST; 
char* BAN_TIME = DEFAULT_BAN_TIME;     

int  (*relay_send)();
int  (*relay_close)();
void (*relay_pause_recv)();


//form all args into one single redis command
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

void handle_error(struct sock_info* identifier){
  if(r != NULL){
    freeReplyObject(r);
  }
  redisFree(c);
  (*relay_close)(identifier);
  LOG("%s[ANTI-BRUTE-FORCE] Error Occurred.%s",MARK_ERROR,MARK_SUFFIX);
}


void phase_ports(char* str){
  char* delim = ",";
  char* p =strtok(str,delim);
  if(!p){
    printf("in");
    ports[port_num++] = atoi(str);
        
    return;
  }
  ports[port_num++] = atoi(p);
  while((p=strtok(NULL,delim))){
    if(port_num == sizeof(ports)/sizeof(int)){
      // printf("%ld\n",malloc_usable_size(ports)/sizeof(int)+PORTS_ADD);
      int* new_ptr = (int*)realloc(ports,(port_max_num+PORTS_ADD)*sizeof(int));
      port_max_num += PORTS_ADD;
      if(!new_ptr){
        LOG("%s[ANTI-BRUTE-FORCE] Fail to Phase Ports.%s",MARK_ERROR,MARK_SUFFIX);
        exit(0);
      }
      ports = new_ptr;
    }
    ports[port_num++] = atoi(p);
  }

  LOG("[ANTI-BRUTE-FORCE] Ports Monitored (%d): %s",port_num,MARK_INFO);
  for(int i=0;i<port_num;i++){
    printf("%d ",ports[i]);
  }
  printf("%s\n",MARK_SUFFIX);
}


void on_init(struct init_info* info) {

  relay_send = info->relay_send;
  relay_close = info->relay_close;
  relay_pause_recv = info->relay_pause_recv;

  int argc = info->argc;
  char** argv = info->argv;
  ports = (int*)malloc(sizeof(int)*PORTS_ADD);
  port_max_num += PORTS_ADD;
      
  int flag = 0;
  for (int i = 0; i < argc; i++) {
    if (i != argc - 1 && (!strcmp(argv[i], "--abf-monitor")||!strcmp(argv[i], "--abf-m"))) {
      MONITOR_TIME = argv[i + 1];
    }
    if (i != argc - 1 && (!strcmp(argv[i], "--abf-request")||!strcmp(argv[i], "--abf-r"))) {
      MAX_REQUEST = atoi(argv[i + 1]);
    }
    if (i != argc - 1 && (!strcmp(argv[i], "--abf-ban")||!strcmp(argv[i], "--abf-b"))) {
      BAN_TIME = argv[i + 1];
    }
    if (i != argc - 1 && (!strcmp(argv[i], "--abf-ports")||!strcmp(argv[i], "--abf-p"))) {
      flag = 1;
      phase_ports(argv[i + 1]);
    }
  }
  if(flag==0){
    char str[] = {DEFAULT_PORTS};
    phase_ports(str);
  }
}



void on_connect(struct sock_info* identifier) {
  char* addr = inet_ntoa(((struct sockaddr_in*)(identifier->src_addr))->sin_addr);
  int dst_port = ntohs(((struct sockaddr_in*)(identifier->dst_addr))->sin_port);
  //LOG("[ANTI-BRUTE_FORCE] Request From:%s, Dst Port:%d",addr,dst_port);

  for(int i=0;i<port_num;i++){
    if(ports[i] == dst_port){break;}
    if(i==port_num-1){return;}
  }

  char cmd[MAX_CMD_LEN];
  c = redisConnect(REDIS_SERVER_ADDR,REDIS_SERVER_PORT);
  if(c->err){ 
    LOG("%s[ANTI-BRUTE-FORCE] Fatal Error : Cannot Connect to Redis Server.%s",MARK_ERROR,MARK_SUFFIX);
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
      LOG("%s[ANTI-BRUTE-FORCE] IP Banned Temporarily: %s%s",MARK_PREFIX,addr,MARK_SUFFIX);
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
  LOG("%s[ANTI-BRUTE-FORCE] Failed to Execute Command: %s",MARK_ERROR,cmd,MARK_SUFFIX);
  handle_error(identifier);
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






