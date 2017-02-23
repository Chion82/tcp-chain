TCP Chain
---------
TCP Chain是一个可拓展、高性能的TCP网络钩子框架（中间件）。通过使用框架提供的API开发插件，可快速实现自定义网络钩子或代理程序。

[插件开发指南](./docs/plugin_dev_guide.md)

* 使用C语言开发，支持Linux平台
* 转发性能高，默认透明代理loopback速率超过400MB/s
* 拓展性强，插件API友好
* 支持双向流量（出站/入站）
* 依赖库：`libev-devel`

已实现插件：
* Logger（示例插件）
* Direct（默认插件，透明代理，默认的数据包转发行为）

计划中：
* WAF（Web应用防护）Demo
* 远程登录暴力破解检测
* 加密隧道转发策略
* ...

编译和使用
--------
* 编译依赖：`libev-devel`（部分发行版package名为`libevdev`或`libev-dev`）
* 编译：
```
$ make
```

* 添加iptables转发规则（此示例仅处理入站请求，服务端口为主机上现有的服务器程序监听端口）：
```
# iptables -t nat -A PREROUTING -p tcp -d [主机IP] --dport [服务端口] -m mark ! --mark 100 -j REDIRECT --to-port 3033
# iptables -t nat -A OUTPUT -p tcp -d [主机IP] --dport [服务端口] -m mark ! --mark 100 -j REDIRECT --to-port 3033
```

* 运行`tcp-chain`（需要`sudo`或在root下运行）：
```
# ./tcp-chain
```

### 可选运行参数
* `--port [PORT]` 监听端口，默认为`3033`
* `--plugin-dir [PLUGIN_DIR]` 插件目录，默认为`./plugins`
* `--direct-mark [DIRECT_MARK]` 透明代理向目的主机连接时使用的iptables mark value (nfmark)，默认为`100`
