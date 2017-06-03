#!bin/bash  

#
# [ HOW TO RUN ] 
#   1. bash RUN.sh [options...]   
#   2. just wait until the server is on
# 


dependency=(
	"libev-dev"
	"libssl-dev"
	"iptables"
	"redis-server"
	#add here
)


user=`whoami`
if [ "$user" != "root" ]; then
    sudo root
fi


# check dependencies
echo ">>>[Check Dependencies]>>>>>>>>>>>>>>>>>>>>>>>>>>>"
for (( i = 0; i < ${#dependency[*]}; i++ )); do
	name=${dependency[i]}
	printf "> %-30s\r" name
	eval dpkg -s "$name" >/dev/null
	if [[ $? -eq 0 ]]; then
		printf "> %-30s[installed]\r" $name
	else
		printf "> %-30s[not installed]\r" $name
		sudo apt-get install "$name"
		if [[ $? > 0 ]]; then
			echo "\033[01;31mAEGIS: apt-get error. Please check network condition and apt-get version.\033[0m"
			exit 
		fi
	fi
	echo
done

# other settings
echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
gnome-terminal -x bash -c "redis-server;" >/dev/null
echo "> redis server is on" 
sudo iptables -t nat -A PREROUTING -p tcp -d 127.0.0.1 --dport 80 -m mark ! --mark 100 -j REDIRECT --to-port 3033
sudo iptables -t nat -A OUTPUT -p tcp -d 127.0.0.1 --dport 80 -m mark ! --mark 100 -j REDIRECT --to-port 3033
echo "> NAT redirected"

#launch Ageis
echo ">>>[Ageis]>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
eval sudo ./aegis $@
