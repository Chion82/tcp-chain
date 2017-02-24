TCP Chain插件开发快速指南
-----------------------
> 你可以参考 [Logger示例插件](../plugins/logger.c) 的源码快速上手。

1\. 在`plugins`目录下新建`hello_plugin.c`。

2\. 引入 `tcp_chain.h` 头文件。
```C
# include "../tcp_chain.h"
```

3\. 定义并实现`on_init()`函数，该函数在插件初始化时被回调（仅在主程序启动时回调一次）：
```C
void on_init(struct init_info* info) {
  LOG("Plugin loaded. Plugin ID is %d.", info->plugin_id);
}
```
`on_init()`被回调时会传入一个`struct init_info`结构，包含了插件初始化时所需要的信息，定义如下：
```C
struct init_info {
  struct ev_loop* default_loop;  //libev的默认loop
  int plugin_id;                 //插件ID
  int (*relay_send)();           //relay_send()的函数指针（见API文档）
  int (*relay_close)();          //relay_close()的函数指针
  void (*relay_pause_recv)();    //relay_pause_recv()的函数指针
  int argc;                      //main()传入的int argc
  char** argv;                   //main()传入的char* argv[]
};
```

4\. 定义并实现`on_connect()`钩子函数，该函数在每个TCP连接建立时被回调：
```C
void on_connect(struct sock_info* identifier) {
  LOG("New connection, relay ID is %d.", identifier->relay_id);
}
```
其中，传入的`struct sock_info`结构包含了标识每个TCP连接的信息，定义如下：
```C
struct sock_info {
  int relay_id;               //标识该TCP连接的ID（注意不是唯一的ID，在该TCP连接关闭后，该ID的值会被复用）
  int plugin_id;              //插件ID
  int* takeovered;            //若 *takeovered==1，表明该连接已被其他插件“最终处理”（代理/转发）
  void* data;                 //用户指针，默认值为NULL，可修改data的指向来关联该TCP连接的自定义数据
  void* shared_data;          //指向一块公共缓存区，大小为2048字节，用于在多个插件之间共享数据。除非realloc()调用，请勿修改shared_data的指向（待进一步讨论）
  struct sockaddr* src_addr;  //TCP连接的原地址
  struct sockaddr* dst_addr;  //TCP连接的目的地址
};
```

5\. 定义并实现`on_recv()`钩子函数，该函数在TCP连接接收到数据时被回调：
```C
void on_recv(struct sock_info* identifier, char** p_data, size_t* length) {
  LOG("Connection %d received %d bytes.", identifier->relay_id, *length);
}
```
函数传入参数定义：
* `struct sock_info* identifier` 同上，标识该TCP连接的信息
* `char** p_data` 指向接收数据缓冲区指针的指针（注意是指针的指针）
* `size_t* length` 指向接收数据长度的指针  
具体注意事项请参考API文档

6\. 定义并实现`on_send()`钩子函数，该函数在TCP连接发送数据前被回调。函数传入参数同`on_recv()`：
```C
void on_send(struct sock_info* identifier, char** p_data, size_t* length) {
  LOG("Connection %d sent %d bytes", identifier->relay_id, *length);
}
```

7\. 定义并实现`on_close()`钩子函数，该函数在TCP连接关闭时被回调：
```C
void on_close(struct sock_info* identifier) {
  LOG("Connection %d closing.", identifier->relay_id);
}
```

8\. **非代理型插件请忽略** 代理插件需要响应主程序的节流请求，实现`pause_remote_recv()`方法，在该方法中暂停远端连接的`recv()`操作。

9\. 其他主动行为操作（详见API文档）：

* `relay_send()` 主动发送数据。
* `relay_close()` 主动关闭TCP连接。
* `relay_pause_recv()` 暂停主程序在该TCP连接的`recv()`操作（用于代理型插件进行节流）

10\. 编译插件：
* 在`plugins/Makefile`最后新增两行：
```
${CC} -fPIC -c hello_plugin.c
${CC} -shared hello_plugin.o -o 90-hello_plugin.so
```

其中，`90-hello_plugin.so`为编译后的插件文件，文件名以数字`XX-`开头，该数字代表插件优先级，数字越小优先级越大，必须为2位数，如`01-pre_processor.so`。
* 在`plugins`目录下运行`make build`编译插件，或在项目根目录下运行`make`编译主程序和全部插件。

11\. 运行`tcp_chain`主程序即可自动加载插件。
