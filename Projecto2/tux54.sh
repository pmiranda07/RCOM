#!/bin/bash

ifconfig eth0 172.16.50.254/24
ifconfig eth1 172.16.51.253/24
route add default gw 172.16.51.254

echo 1 >/proc/sys/net/ipv4/ip_forward
echo 0 >/proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

iptables –t nat –A POSTROUTING -o eth1 –j MASQUERADE
iptables –A FORWARD –i eth1 –m state --state NEW,INVALID –j DROP
iptables –L
iptables –t nat –L 
