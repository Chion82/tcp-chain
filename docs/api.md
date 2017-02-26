TCP Chain API文档
-----------------

### 插件钩子函数

插件开发者需要实现这部分钩子函数。

* `void on_init(struct init_info* info)`
  * 插件初始化时被回调
  * 参数：
    - `struct init_info* info` 插件初始化信息  
  * 返回值：无

* `void on_connect(struct sock_info* identifier)`
  * 新TCP连接建立时被回调
  * 参数：
    - `struct sock_info* identifier` TCP连接标识信息
  * 返回值： 无

* `void on_recv(struct sock_info* identifier, char** p_data, size_t* length)`
  * TCP连接接收到数据时被回调
  * 参数：
    - `struct sock_info* identifier` TCP连接标识信息
    - `char** p_data` 指向接收数据缓冲区指针的指针（注意是指针的指针）**除非`realloc()`调用，请勿修改`*p_data`的指向**
    - `size_t* length` 指向接收数据长度的指针
  * 返回值： 无
  * 当需要增加缓冲区的容量时，使用`realloc()`调用：
  ```C
  size_t new_length = *length + 16;
  *p_data = realloc(*p_data, new_length);
  *length = new_length;
  ```

* `void on_send(struct sock_info* identifier, char** p_data, size_t* length)`
  * TCP连接发送数据时被回调
  * 参数：
    - `struct sock_info* identifier` TCP连接标识信息
    - `char** p_data` 指向发送数据缓冲区指针的指针（注意是指针的指针）**除非`realloc()`调用，请勿修改`*p_data`的指向**
    - `size_t* length` 指向发送数据长度的指针
  * 返回值： 无
  * 当需要增加缓冲区的容量时，使用`realloc()`调用：
  ```C
  size_t new_length = *length + 16;
  *p_data = realloc(*p_data, new_length);
  *length = new_length;
  ```

* `void on_close(struct sock_info* identifier)`
  * TCP连接关闭时被回调
  * 参数：
    - `struct sock_info* identifier` TCP连接标识信息
  * 返回值： 无

* `void pause_remote_recv(struct sock_info* identifier, int pause)`
  * 用于代理型插件响应主程序的节流请求（窗口控制），非代理型插件不需要实现该函数。在该函数中，需要根据传入参数`pause`的值暂停/继续远端的`recv()`操作。
  * 参数：
    - `struct sock_info* identifier` TCP连接标识信息
    - `int pause` 若`pause==1`，暂停远端`recv()`；若`pause==0`，继续远端`recv()`
  * 示例（Direct透明代理部分源码，使用libev）：
  ```C
  void pause_remote_recv(struct sock_info* identifier, int pause) {
    struct proxy_wrap* proxy = (struct proxy_wrap*)(identifier->data);
    if (pause) {
      ev_io_stop(default_loop, &((proxy->read_io).io));
    } else {
      ev_io_start(default_loop, &((proxy->read_io).io));
    }
  }
  ```

### 主动操作

主动操作的函数指针在`on_init()`被回调时通过`info`参数传入。

* `int relay_send(struct sock_info* identifier, char *buffer, size_t length, int flags)`
  * 向TCP连接发送数据
  * 参数：
    - `struct sock_info* identifier` TCP连接标识信息
    - `char *buffer` 发送数据缓冲区
    - `size_t length` 发送数据的长度
    - `int flags` 保留参数，置`0`即可
  * 返回值：
    - 如果发送失败则返回`-1`（此时会立即关闭TCP连接，`on_close()`会被调用），否则返回发送数据的长度。

* `int relay_close(struct sock_info* identifier)`
  * 关闭TCP连接
  * 参数：
    - `struct sock_info* identifier` TCP连接标识信息
  * 返回值：
    - 成功返回`0`，否则返回`-1`

* `void relay_pause_recv_func(struct sock_info* identifier, int pause)`
  * 用于代理型插件请求主程序节流（窗口控制）。
  * 参数：
    - `struct sock_info* identifier` TCP连接标识信息
    - `int pause` 若为`1`，则暂停TCP连接的`recv()`操作；若为`0`，则继续TCP连接的`recv()`操作。
  * 返回值：
    - 无

### 结构定义

* `struct init_info`
  ```C
  struct init_info {
    struct ev_loop* default_loop;  //[readonly] libev的默认loop
    int plugin_id;                 //[readonly] 插件ID
    int (*relay_send)();           //[readonly] relay_send()的函数指针（见API文档）
    int (*relay_close)();          //[readonly] relay_close()的函数指针
    void (*relay_pause_recv)();    //[readonly] relay_pause_recv()的函数指针
    int argc;                      //[readonly] main()传入的int argc
    char** argv;                   //[readonly] main()传入的char* argv[]
  };
  ```

* `struct sock_info`
  ```C
  struct sock_info {
    int relay_id;               //[readonly] 标识该TCP连接的ID（注意不是唯一的ID，在该TCP连接关闭后，该ID的值会被复用）
    int plugin_id;              //[readonly] 插件ID
    int* takeovered;            //若 *takeovered==1，表明该连接已被其他插件“最终处理”（代理/转发）
    void* data;                 //用户指针，默认值为NULL，可修改data的指向来关联该TCP连接的自定义数据
    void* shared_data;          //[readonly] 指向一块公共缓存区，大小为2048字节，用于在多个插件之间共享数据（公共缓冲区spec约定有待讨论）。
    struct sockaddr* src_addr;  //[readonly] TCP连接的原地址
    struct sockaddr* dst_addr;  //[readonly] TCP连接的目的地址
  };
  ```

### 其它约定

* 避免使用多线程/进程。
* 使用非阻塞IO，推荐配合libev，使用`struct init_info`提供的`default_loop`作为IO loop和timer loop。
* 代理型插件在执行代理前应判断`*(identifier->takeovered)`是否为`1`，若为`1`则表明该TCP连接已被其它代理型插件接手而本插件无需执行代理；否则将该值置为`1`后再执行代理。
* 宏`BUFFER_SIZE`定义为`2048`，指示单次收发数据的缓冲区容量。
* TCP是基于stream的，对于需要识别收发payload的插件，要在每次`on_recv/send()`回调中对收发buffer的数据进行增量缓存。
* 插件间共享缓冲区`identifier->shared_data`，格式约定暂未确定，有待讨论。
