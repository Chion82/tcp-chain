sudo iptables -t nat -A PREROUTING -p tcp -d 127.0.0.1 --dport 80 -m mark ! --mark 100 -j REDIRECT --to-port 3033
sudo iptables -t nat -A OUTPUT -p tcp -d 127.0.0.1 --dport 80 -m mark ! --mark 100 -j REDIRECT --to-port 3033
./../redis-3.2.8/src/redis-server
