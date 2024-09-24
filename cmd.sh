#服务器收数据
./tcp_udp -w 32768 -t 12 -l 169.254.144.1 5555 >r1 </dev/null

#客户端发数据
./tcp_udp -w 32768 -t 12 169.254.144.1 5555 >/dev/null <s1


#创建数据
dd status=none if=/dev/urandom of=s1 bs=1000000 count=1


#[cmd begin]./apps/tcp_udp -t 12 -w 32768 -l 169.254.144.1 9980 [cmd end]
#
#[cmd begin]./apps/tcp_udp -t 12 -w 32768 169.254.144.1 9980 [cmd end]

