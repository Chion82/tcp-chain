#!bin/bash  

#
# [ HOW TO TEST ] 
# 1.startup the server (sudo ./tcp_chain --abf-r 1000)     
# 2.type "bash waf_test.sh" on the terminal and then enter
#

sudo iptables -t nat -A PREROUTING -p tcp -d 127.0.0.1 --dport 80 -m mark ! --mark 100 -j REDIRECT --to-port 3033
sudo iptables -t nat -A OUTPUT -p tcp -d 127.0.0.1 --dport 80 -m mark ! --mark 100 -j REDIRECT --to-port 3033
sudo iptables -t nat -A PREROUTING -p tcp -d 127.0.0.1 --dport 20 -m mark ! --mark 100 -j REDIRECT --to-port 3033
sudo iptables -t nat -A OUTPUT -p tcp -d 127.0.0.1 --dport 20 -m mark ! --mark 100 -j REDIRECT --to-port 3033
sudo iptables -t nat -A PREROUTING -p tcp -d 127.0.0.1 --dport 21 -m mark ! --mark 100 -j REDIRECT --to-port 3033
sudo iptables -t nat -A OUTPUT -p tcp -d 127.0.0.1 --dport 21 -m mark ! --mark 100 -j REDIRECT --to-port 3033


success=0;
arr_cmd=(
	#1-5 test server
	"curl -s 127.0.0.1"
	"curl -s localhost/index.nginx-debian.html"
	"curl -s localhost"
	"curl -s localhost/index.html"
	"curl -s localhost:21"
	#6-10 test args
	"curl -s localhost?id=1"
	"curl -s localhost?file://pwd"
	"curl -s localhost?select.from"
	"curl -s localhost/index.php?id=1\&oe=utf8\&ie=utf8\&source=uds\&hl=zh-CN&q=qq"
	"curl -s localhost?id=1\&user=admin\&base64_decode"
	# #11-15 test user agent
	"curl -s localhost"
	"curl -s localhost --user-agent 'Mozilla 5.0'"
	"curl -s localhost --user-agent WebKit"
	"curl -s localhost --user-agent HTTrack"
	"curl -s localhost?user-agent=curl --user-agent Parser"
	#16-20 test url
	"curl -s localhost/index.html"
	"curl -s localhost/user/id/123456?isAuthenticated=true"
	"curl -s localhost/phpmyadmin/"
	"curl -s localhost/index.nginx-debian.html/"
	"curl -s localhost/www/s.rar"
	#21-25 test post data
	"curl -s localhost -d pwd=123456"
	"curl -s localhost -d pwd=1\&token=a6bc2902c8e172d3\&'into dumpfile'"
	"curl -s localhost?name=god -d table=1"
	"curl -s localhost?name=god\&file:/ -d cheated=true\&gopher:/"
	"curl -s localhost/index.nginx-debian.html?id=0 -d pwd=null\&union.select --user-agent HTTrack"
	#26-30 test cookie
	"curl -s localhost --cookie Username=admin;"
	"curl -s localhost --cookie 'Include=include();Expires=Wednesday,20-05-2017 00:00:00 GMT'"
	"curl -s localhost --cookie Username=admin;Path=PATH;Domain=domain"
	"curl -s localhost --cookie '' -d file://"
	"curl -s localhostindex.nginx-debian.html?id=0 --cookie Path=/etc/passwd/123"
	)

arr_expected=(
	#1-5
	0 0 0 0 -1
	#6-10
	0 -1 -1 0 -1
	#11-15
	0 0 0 -1 -1
	#16-20
	0 0 -1 0 -1
	#21-25
	0 -1 0 -1 -1
	#26-30 
	0 -1 0 -1 -1
)

function judge(){
	code=$1;
	index=$2;
	if [ $code -gt 0 ]; then
		let code=-1;
	fi
	if [ $code -eq ${arr_expected[$index]} ]; then
		((success++));
	else
		echo "Not passed: Case" $[$index+1]
	fi
}



x=''
for (( i = 0; i < ${#arr_cmd[*]}; i++ )); do
	eval "${arr_cmd[i]}" >/dev/null;
	judge $? $i;
	printf "Running:[%-29s]%d%%\r" $x $[($i+1)*100/30]
	sleep 0.05
	x=#$x
done

echo
echo "Test Cases:" ${#arr_cmd[*]};
echo "Passed:" $success
rate=$(echo "scale=2;$success/${#arr_cmd[*]}*100.00"|bc)
echo "Pass Rate:" $rate%


