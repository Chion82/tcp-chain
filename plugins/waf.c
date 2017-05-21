/*

Command:
--waf--port     (--waf-p)    :set monitored ports,seperated by ','

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
#include <regex.h>


#include "../tcp_chain.h"


#define PORTS_ADD     10
#define REGEX_ADD     50
#define MAX_CMD_LEN   128
#define BUF_SIZE      256
#define TYPE_COUNT    6
#define NMATCH        10
#define MARK_PREFIX   "\033[01;31m"   //set font red
#define MARK_ERROR    "\033[01;35m"   //set font purple
#define MARK_INFO     "\033[01;34m"   //set font blue
#define MARK_SUFFIX   "\033[0m"       //reset font color
#define DEFAULT_PORTS "80"            //string,seperated by ','
#define PATH_PREFIX   "./conf/waf/"

typedef struct Regex{
  regex_t** regex;
  int line_count;
  int line_max;
}Regex;

int  (*relay_send)();
int  (*relay_close)();
void (*relay_pause_recv)();

Regex args_regex;
Regex cookie_regex;
Regex post_regex;
Regex url_regex;
Regex useragent_regex;
Regex whiteurl_regex;
Regex* regex_set;

regmatch_t pmatch[NMATCH];
const size_t nmatch = NMATCH;
int match_status;

char** get_or_post;
char** head;
char** args;
char** cookie;
char** post;
char** url;
char** useragent;
char** whiteurl;

int port_num = 0;
int port_max_num = 0;
int* ports;

int is_relay_closed = 0;
int mark_flag;


void regex_init(){
  Regex temp[TYPE_COUNT] = {args_regex,cookie_regex,post_regex,url_regex,useragent_regex,whiteurl_regex};;
  regex_set = (Regex*)temp;
  for(int i=0;i<TYPE_COUNT;i++){
    regex_set[i].line_count = 0;
    regex_set[i].line_max = 0;
  }
}

void read_regex_conf(Regex* dst, char* filename){

  char path[50] = PATH_PREFIX;
  strcat((char*)path,filename);
  FILE* file;
  if((file = fopen((char*)path,"r")) == NULL){
    LOG("%s[WEB-APP-FIREWALL] Cannot Read RegEx Config File: %s.%s",MARK_ERROR,filename,MARK_SUFFIX);
    dst = NULL;
    return;
  }

  char buf[BUF_SIZE];
  while(fgets(buf,BUF_SIZE,file) != NULL){
    if(dst->line_count == dst->line_max){
      dst->line_max += REGEX_ADD;
      regex_t** new_ptr = (regex_t**)realloc(dst->regex,(dst->line_max)*sizeof(regex_t*));
      if(!new_ptr){
        LOG("%s[WEB-APP-FIREWALL] Cannot Read RegEx Configuration.%s",MARK_ERROR,MARK_SUFFIX);
        exit(0);
      }
      dst->regex = new_ptr;
    }

    buf[strlen(buf)-1] = '\0';
    regex_t* preg = (regex_t*)malloc(sizeof(regex_t));
    int status = regcomp(preg,(char*)buf,REG_EXTENDED);        
    dst->regex[dst->line_count++] = preg;
  }
  fclose(file);
}

void mark_red(char** psrc, int num, int* so, int* eo){
  char marked[BUF_SIZE] = "";
  char* src = *psrc;
  int temp;

  for(int i=0;i<num-1;i++){
    for(int j=0;j<num-i-1;j++){
      if(so[j]>so[j+1]){
        temp=so[j]; so[j]=so[j+1]; so[j+1]=temp;
        temp=eo[j]; eo[j]=eo[j+1]; eo[j+1]=temp;
      }
    }
  }

  strncat(marked,src,so[0]);
  strcat(marked,MARK_PREFIX);
  for(int i=0;i<num-1;i++){
    strncat(marked,src+so[i],eo[i]-so[i]);
    strcat(marked,MARK_SUFFIX);
    strncat(marked,src+eo[i],so[i+1]-eo[i]);
    strcat(marked,MARK_PREFIX);
  }
  strncat(marked,src+so[num-1],eo[num-1]-so[num-1]);
  strcat(marked,MARK_SUFFIX);
  strcat(marked,src+eo[num-1]);

  //printf("%s\n", marked);
  strcpy(*psrc,marked);
}

void match(char** psrc, Regex* p, struct sock_info* identifier){

  char* src = *psrc;
  if(src == NULL){
    return;
  }

  if(src[strlen(src)-1]=='\n'|| src[strlen(src)-1]=='\r'){
    src[strlen(src)-1]='\0';
  }

  int so[NMATCH];
  int eo[NMATCH];
  int num = 0;
  mark_flag = 0;

  for(int i=0;i<p->line_count;i++){

    for(int j=0;j<NMATCH;j++){
      pmatch[j].rm_so = -1;
      pmatch[j].rm_eo = -1;
    }

    match_status = regexec(p->regex[i],src,nmatch,pmatch,0);
    // printf("regexec status:%d\n",match_status);
    if(match_status == 0){
      if(!is_relay_closed){
        char* addr = inet_ntoa(((struct sockaddr_in*)(identifier->src_addr))->sin_addr);
        int dst_port = ntohs(((struct sockaddr_in*)(identifier->dst_addr))->sin_port);
        LOG("%s[WEB-APP-FIREWALL] Aggressive Segment Detected (:%d), Discard. (From %s)%s",MARK_PREFIX,dst_port,addr,MARK_SUFFIX);
        (*relay_close)(identifier);
        is_relay_closed = 1;
      }

      mark_flag = 1;
      if(num>=NMATCH){goto MARK;}
      if(pmatch[0].rm_so>=0){
        so[num] = pmatch[0].rm_so;
        eo[num++] = pmatch[0].rm_eo;
      }

    }
  }

MARK: 
  if(mark_flag){
    mark_red(psrc,num,so,eo);
  }
  return;
}

void get_post_data(char** dst, char* src){

  char temp[strlen(src)+1];
  strcpy(temp,src);
  if(*dst!=NULL){free(*dst);}
  *dst = (char*)malloc(sizeof(char)*BUF_SIZE);

  char* delim = "\n";
  char** saveptr = (char**)malloc(sizeof(char*));
  char* p = strtok_r(temp,delim,saveptr); 
  while(p){
    if(strlen(p)==1){
      strcpy(*dst,*saveptr);
      free(saveptr);
      // printf("%d\n", strlen(*dst));
      if(strlen(*dst)==0){break;}
      return;
    }
    p = strtok_r(*saveptr,delim,saveptr);
  }
  *dst = NULL;
}

void get_request_line(char** dst, char* src){
  // char temp[strlen(src)+1];
  // strcpy(temp,src);
  if(*dst!=NULL){free(*dst);}
  *dst = (char*)malloc(sizeof(char)*BUF_SIZE);

  char* delim = " ";
  char* p;
  char request_line[BUF_SIZE];

  sscanf(src,"%[^\n]",request_line);
  if(request_line){
    p = strtok(request_line,delim);
    if(p){
      p = strtok(NULL,delim);
      if(p){
        strcpy(*dst,p);
        return;
      }
    }    
  }
  *dst = NULL;
}

void get_value_by_key(char** dst, char* src, char* key){

  char temp[strlen(src)+1];
  strcpy(temp,src);
  if(*dst!=NULL){free(*dst);}
  *dst = (char*)malloc(sizeof(char)*BUF_SIZE);

  int flag = 0;
  char* delim1 = "\n";
  char* delim2 = " ";
  char* current_key;
  char** saveptr1 = (char**)malloc(sizeof(char*));
  char** saveptr2 = (char**)malloc(sizeof(char*));
  char* p = strtok_r(temp,delim1,saveptr1); 
  while(p){
    current_key = strtok_r(p,delim2,saveptr2);   
    if(current_key){
      if(current_key[strlen(current_key)-1]==':'){
        current_key[strlen(current_key)-1]= '\0';
      }
      if(strcmp(key,current_key)==0){
        strcpy(*dst,*saveptr2);
        goto FREE;
      }
    }
    p = strtok_r(*saveptr1,delim1,saveptr1);
  }
  *dst = NULL;

FREE:
  free(saveptr1);
  free(saveptr2);

}

void get_url_and_args(char** url, char** args, char* str){
    
  char* delim1 = "?";
  char* delim2 = " ";
  char* p = strtok(str,delim1);
  if(!p){
    *url = strtok(str,delim2);
    *args = NULL;
  }
  else{
    *url = p; 
    *args = strtok(NULL,delim2);
  }
}


void phase_ports(char* str){
  char* delim = ",";
  char* p =strtok(str,delim);
  if(!p){
    ports[port_num++] = atoi(str); 
    return;
  }
  ports[port_num++] = atoi(p);
  while((p=strtok(NULL,delim))){
    if(port_num == sizeof(ports)/sizeof(int)){
      int* new_ptr = (int*)realloc(ports,(port_max_num+PORTS_ADD)*sizeof(int));
      port_max_num += PORTS_ADD;
      if(!new_ptr){
        LOG("%s[WEB-APP-FIREWALL] Fail to Phase Ports.%s",MARK_ERROR,MARK_SUFFIX);
        exit(0);
      }
      ports = new_ptr;
    }
    ports[port_num++] = atoi(p);
  }

  LOG("[WEB-APP-FIREWALL] Ports Monitored (%d): %s",port_num,MARK_INFO);
  for(int i=0;i<port_num;i++){
    printf("%d ",ports[i]);
  }
  printf("%s\n",MARK_SUFFIX);
}

void console(){
  //printf("GET||POST:%s\n", *get_or_post);
  LOG("************************** Request Segment *************************");
  LOG("  User-Agent:\t%s", *useragent);
  LOG("  Cookie:\t\t%s", *cookie);
  LOG("  Url:\t\t%s", *url);
  LOG("  Args:\t\t%s", *args);
  LOG("  Data:\t\t%s", *post);
  LOG("********************************************************************");
}


void on_init(struct init_info* info) {
setbuf(stdout,NULL);
  relay_send = info->relay_send;
  relay_close = info->relay_close;
  relay_pause_recv = info->relay_pause_recv;

  int argc = info->argc;
  char** argv = info->argv;

  ports = (int*)malloc(sizeof(int)*PORTS_ADD);
  port_max_num += PORTS_ADD;
      
  int flag = 0;
  for (int i = 0; i < argc; i++) {
    if (i != argc - 1 && (!strcmp(argv[i], "--waf--port")||!strcmp(argv[i], "--waf-p"))) {
      flag = 1;
      phase_ports(argv[i + 1]);
    }
  }
  if(flag==0){
    char str[] = {DEFAULT_PORTS};
    phase_ports(str);
  }

  get_or_post = (char**)malloc(sizeof(char*));      *get_or_post  = (char*)malloc(sizeof(char)*BUF_SIZE);
  head        = (char**)malloc(sizeof(char*));      *head         = (char*)malloc(sizeof(char)*BUF_SIZE);
  args        = (char**)malloc(sizeof(char*));      *args         = (char*)malloc(sizeof(char)*BUF_SIZE);
  cookie      = (char**)malloc(sizeof(char*));      *cookie       = (char*)malloc(sizeof(char)*BUF_SIZE);
  post        = (char**)malloc(sizeof(char*));      *post         = (char*)malloc(sizeof(char)*BUF_SIZE);
  url         = (char**)malloc(sizeof(char*));      *url          = (char*)malloc(sizeof(char)*BUF_SIZE);
  useragent   = (char**)malloc(sizeof(char*));      *useragent    = (char*)malloc(sizeof(char)*BUF_SIZE);
  whiteurl    = (char**)malloc(sizeof(char*));      *whiteurl     = (char*)malloc(sizeof(char)*BUF_SIZE);

  regex_init();
  read_regex_conf(&useragent_regex,"user-agent");
  read_regex_conf(&args_regex,"args");
  read_regex_conf(&url_regex,"url");
  read_regex_conf(&post_regex,"post");
  read_regex_conf(&cookie_regex,"cookie");

}

void on_connect(struct sock_info* identifier) {}

void on_send(struct sock_info* identifier, char** p_data, size_t* length) {}

void on_close(struct sock_info* identifier) {}

void on_recv(struct sock_info* identifier, char** p_data, size_t* length) {

  is_relay_closed = 0;
  int dst_port = ntohs(((struct sockaddr_in*)(identifier->dst_addr))->sin_port);
  for(int i=0;i<port_num;i++){
    if(ports[i] == dst_port){break;}
    if(i==port_num-1){return;}
  }

  get_value_by_key(useragent,*p_data,"User-Agent");
  get_value_by_key(cookie,*p_data,"Cookie");
  get_request_line(get_or_post,*p_data);
  get_post_data(post, *p_data);
  get_url_and_args(url, args, *get_or_post);

  match(post,&post_regex,identifier);
  match(useragent,&useragent_regex,identifier);
  match(cookie,&cookie_regex,identifier);
  match(url,&url_regex,identifier);
  match(args,&args_regex,identifier);

  if(is_relay_closed){
    console();
  }
  
  return;
}



