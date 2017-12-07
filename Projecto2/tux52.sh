#!/bin/bash

ifconfig eth0 172.16.51.1/24
route add default gw 172.16.51.254
route add -net 172.16.50.0/24 gw 172.16.51.253

echo 0 >/proc/sys/net/ipv4/conf/eth0/accept_redirects
echo 0 >/proc/sys/net/ipv4/conf/all/accept_redirects
echo 1 >/proc/sys/net/ipv4/conf/eth0/accept_redirects
echo 1 >/proc/sys/net/ipv4/conf/all/accept_redirects

