#!bin/bash  

#
# [ HOW TO TEST ] 
# 1.startup the server (sudo ./tcp_chain --abf-r 100)     
# 2.type "bash abf_test.sh" on the terminal and then enter
#

sudo iptables -t nat -A PREROUTING -p tcp -d 127.0.0.1 --dport 80 -m mark ! --mark 100 -j REDIRECT --to-port 3033
sudo iptables -t nat -A OUTPUT -p tcp -d 127.0.0.1 --dport 80 -m mark ! --mark 100 -j REDIRECT --to-port 3033


x=''
count=0
dot_count=0
status=1
while [[ $status -eq 1 ]]; do
	sleep 0.1;
	curl -s 127.0.0.1 >/dev/null
	if [[ $? -eq 0 ]]; then
		((count++))
		((dot_count++))
	else
		let status=0;
	fi

	if [[ $dot_count -eq 5 ]]; then
		x=$x.
		let dot_count=0;
	fi

	printf "Running%s\r" $x
done

echo
echo "Test Result: IP Banned"
elapse=$(echo "scale=1;$count*0.1"|bc)
echo "Frequency:" $count "requests in "$elapse "sec"



