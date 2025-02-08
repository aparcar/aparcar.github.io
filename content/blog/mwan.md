---
title: "Multi-WAN over WireGuard on OpenWrt"
date: 2025-02-08T16:01:23Z
draft: true
---

For the last two years I ogranized the IT infrastructure of a small festival, including providing internet access for the crew. The festival is located in a rural area, specifically an old airport on a hill, with now celluar coverage and no fixed internet connection. The only way reasonable way to provide Internet access is via satellite, namely Starlink.

Providing uplink for multiple hundgred crew members in the weeks before the festival revealed two challenges for the uplink:

* The satellite connection is limited in speed causing mediocre performance
* The borrowed Starlink is legally bound to a single person, may causing issue if i.e. torrents are downloaded

The first challange can be solved by using multi-WAN and multiple starlinks, the latter by using a VPN tunnel. Combining the two was not trivial and is the topic of this blog post. Since a single device aka box does those two things, I'll refer to it as a Magic Box.

## Requirements

To run the setup you need at least one device running OpenWrt and one endpoint with a public IP address, i.e. a VPS. During testing three uplinks were used with equal speed, simulating three Starlink connections. The setup should also work with a mix of uplink connections, i.e. DSL, LTE and satellite, however would require additional configuration, more on that later.

## Setup

This setup describes a OpenWrt device with three WAN uplinks and one LAN interface. While tested in a virtual machine, all configuration can be directly applied to a physical device, i.e. an [Banana Pi BPI-R4](https://wiki.banana-pi.org/Banana_Pi_BPI-R4), which should have enough routing for the real-world setup.

### WireGuard (Server)

This step did cost me the most time since I did not verify the WireGuard setup on the VPS endpoint. While I followed [my own tutorial](https://aparcar.org/openwrt-with-wireguard-vpn/) and everything seemed to work fine, a single line of `iptables` configuration was missing on the VPS[^iptables].

You'll need the same number of WireGuard tunnels as you plan to use uplinks. In this case three tunnels were created, each tunnel using a different WAN interface.

| Interface | Server IP   | Magic Box IP | Endpoint Port |
|-----------|-------------|--------------| -------------------- |
| wg1       | 10.0.1.1/24 | 10.0.1.2/32  | `<IP>:30001`                |
| wg2       | 10.0.2.1/24 | 10.0.2.2/32  | `<IP>:30002`                |
| wg3       | 10.0.3.1/24 | 10.0.3.2/32  | `<IP>:30003`                |

> In this setup all WireGuard tunnels use the same VPS as endpoint, however it is possible to use different endpoints for better redundancy. However, this would lead to different public IP addresses while surfing the web, which might cause issues with some services.

### Firewall tables

Each WireGuard tunnel should only use a single uplink. This is done by using `iptables` to mark the traffic and later use the mark to route the traffic to the correct uplink. To create the required tables and chains, add the following lines to `/etc/iproute2/rt_tables`:

```shell
210     wan1
211     wan2
212     wan3
```

> The numbers are arbitrary, however should be unique and not conflict with existing tables.

### OpenWrt WAN interfaces

The WAN configuration happens as usual in `/etc/config/network` with two additional options:

* `metric` to set the priority of the uplink. The `mwan3` package will later configure the actual interfaces to use, however needs a metric to function. For a three uplink setup, the values `40`, `50` and `60` were used.
* `ip4table` to set the routing table for the uplink. This is required to route the traffic from the WireGuard tunnel to the correct uplink.

```shell
config interface 'wan1'
        option device 'eth1'
        option proto 'dhcp'
        option metric '40'
        option ip4table 'wan1'

config interface 'wan2'
        option device 'eth2'
        option proto 'dhcp'
        option metric '50'
        option ip4table 'wan2'

config interface 'wan3'
        option device 'eth3'
        option proto 'dhcp'
        option metric '60'
        option ip4table 'wan3'
```

### WireGuard (Magic Box)

The WireGuard configuration on the Magic Box follows the same pattern as it's setup on OpenWrt, however with some extra section to route the traffic to the correct uplink. The following configuration is used for the first WireGuard tunnel:

```shell
config interface 'wg1'
        option proto 'wireguard'
        option private_key '<PRIVATE_KEY>'
        option fwmark '0x388'
        option nohostroute '1'
        option listen_port '30001'
        option metric '10'

config wireguard_wg1
        option public_key '<PUBLIC_KEY>'
        list allowed_ips '0.0.0.0/0'
        list allowed_ips '::/0'
        option endpoint_host '<ENDPOINT_PUBLIC_IP>'
        option endpoint_port '30001'
        option persistent_keepalive '25'

config interface 'wg1wan'
        option proto 'static'
        option device 'wg1'
        option ipaddr '10.0.1.2'
        option netmask '255.255.255.0'
        option gateway '10.0.1.1'
        option metric '10'

config rule
        option mark '0x388'
        option lookup 'wan1'

config rule
        option mark '0x388'
        option action 'prohibit'
```

### Firewall

The interfaces `wan1`, `wan2` and `wan3` should be added to the `wan` zone to allow traffic to pass through the firewall and masquerade them (aka NAT). The following lines should be present in `/etc/config/firewall`:

```shell
config zone
        option name 'wan'
        option input 'REJECT'
        option output 'ACCEPT'
        option forward 'REJECT'
        option masq '1'
        option mtu_fix '1'
        list network 'wan1'
        list network 'wan2'
        list network 'wan3'
```

The interfaces `wg1wan`, `wg2wan` and `wg3wan` should be added to a new `wg` zone and forwarding enabled, just like it is for `lan`. The following lines should be added to `/etc/config/firewall`:

```shell
config zone
        option name 'wg'
        option input 'ACCEPT'
        option output 'ACCEPT'
        option forward 'ACCEPT'
        option masq '1'
        list network 'wg2wan'
        list network 'wg3wan'
        list network 'wg1wan'

config forwarding
        option src 'lan'
        option dest 'wg'
```

At this point you should have three WireGuard tunnels, each using it's own uplink. Running `ip route` on the MagicBox should show the following output:

```shell
root@magic:~# ip route
default via 10.0.1.1 dev wg1 proto static metric 10
default via 10.0.2.1 dev wg2 proto static metric 20
default via 10.0.3.1 dev wg3 proto static metric 30
10.0.1.0/24 dev wg1 proto static scope link metric 10
10.0.2.0/24 dev wg2 proto static scope link metric 20
10.0.3.0/24 dev wg3 proto static scope link metric 30
192.168.1.0/24 dev br-lan proto kernel scope link src 192.168.1.1
```

You can already see, `wg1` has the lowest metric and is thereby used as default. You can verify that by running the following command:

```shell
root@magic:~# curl http://my.ip.fi
<ENDPOINT_PUBLIC_IP>
```

### `mwan3`

This obviously requires the `mwan3` package, install it via the following command:

```shell
opkg update
opkg install mwan3
```

The configuration happens in `/etc/config/mwan3` and should look something like this:

Make sure the `rt_table_lookup` values match the ones in `/etc/iproute2/rt_tables`:

```shell
config globals 'globals'
        option mmx_mask '0x3F00'
        option logging '1'
        option loglevel 'notice'
        list rt_table_lookup '210'
        list rt_table_lookup '211'
        list rt_table_lookup '212'
```

A *balanced* policy is used to distribute the traffic over all uplinks. If different uplink speeds would be available, the values of `metric` and `weight` of the `member` sections should be adjusted accordingly.

```shell
config policy 'balanced'
        option last_resort 'unreachable'
        list use_member 'wan1_m1'
        list use_member 'wan2_m2'
        list use_member 'wan3_m3'

config member 'wan1_m1'
        option interface 'wg1wan'
        option metric '10'
        option weight '10'

config member 'wan2_m2'
        option interface 'wg2wan'
        option metric '10'
        option weight '10'

config member 'wan3_m3'
        option interface 'wg3wan'
        option metric '10'
        option weight '10'
```

The default *rules* of the `mwan3` package are used and shown here only for completeness:

```
config rule 'https'
	option sticky '1'
	option dest_port '443'
	option proto 'tcp'
	option use_policy 'balanced'

config rule 'default_rule_v4'
	option dest_ip '0.0.0.0/0'
	option use_policy 'balanced'
	option family 'ipv4'

config rule 'default_rule_v6'
	option dest_ip '::/0'
	option use_policy 'balanced'
	option family 'ipv6'
```

The final section *interface* must be repeated for `wg1wan`, `wg2wan` and `wg3wan`. Different `track_ip`[^track_ips] are used for each interface as the same IP for different interfaces may cause corner case issues. The `flush_conntrack` option is used to remove the connection tracking of the interface when the interface goes offline. The `initial_state` option is used to disable the interface untils it's first successful ping. For the last six options, more *agressive* numbers where used to trigger a failover faster. This should be adopted to the specific use case.

```shell
config interface 'wg1wan'
        option enabled '1'
        option family 'ipv4'
        list track_ip '1.1.1.1'
        list flush_conntrack 'disconnected'
        option initial_state 'offline'
        option timeout '1'
        option interval '3'
        option failure_interval '1'
        option recovery_interval '1'
        option down '3'
        option up '3'
```

Restart `mwan3` to apply the changes:

```shell
/etc/init.d/mwan3 restart
```

### Testing

From the terminal you can check the current interface status via the following command:

```shell
root@magic:~# mwan3 interfaces
Interface status:
 interface wg2wan is online and tracking is active (online 00h:01m:51s, uptime 01h:06m:21s)
 interface wg3wan is online and tracking is active (online 00h:01m:51s, uptime 01h:06m:21s)
 interface wg1wan is online and tracking is active (online 00h:01m:51s, uptime 01h:06m:21s)
```

To test failover, you can disable a WireGuard interface on the server side and see how the MagicBox switches to another uplink. Below is an illustration using the `mtr` tool which constantly pings a server and shows the route to it. Anything but questionmars (`?`) mean the connection was successfull.


```shell
opkg install mtr
mtr 1.1.1.1
```

Stopping the WireGuard tunnel on the server side should show the following output:

```shell
My traceroute  [v0.95]
magic (10.0.1.2) -> 1.1.1.1 (1.1.1.1)                  2025-02-08T17:58:18+0000
Keys:  Help   Display mode   Restart statistics   Order of fields   quit

                             Last  50 pings
 1. 10.0.3.1                 .........................????????.................
 2. [REDACTED]               .........................????????................
 3. [REDACTED]               .........................????????................
 4. [REDACTED]               .........................???????.................
 5. [REDACTED]               .........................???????.................
 6. [REDACTED]               ................?.......????????..>..............
 7. one.one.one.one          ........................????????.................

Scale:  .:74 ms  1:76 ms  2:80 ms  3:86 ms  a:93 ms  b:102 ms  c:113 ms  >
```

The interface status looks now something like this:

```shell
root@magic:~# mwan3 interfaces
Interface status:
 interface wg2wan is offline and tracking is active
 interface wg3wan is online and tracking is active (online 00h:05m:27s, uptime 01h:19m:17s)
 interface wg1wan is offline and tracking is active
```

### Conclusion

This setup was tested on a QEMU virtual machine with three uplinks and a single LAN interface. The setup should work on a physical device with three uplinks, which will be tested in the near future, including some benchmarks.

A followup post at the end of this year will cover how it went on the festival and if the setup brought the expected improvements

### Outlook

In theory *Multipath TCP* (MPTCP) protocol should be able to combine the uplinks and provide a single, faster connection, since it could combine streams from all uplinks. I decided to use `mwan3` since it supports different uplink speeds which isn't did not seem ideal for MPTCP. However, I plan to test MPTCP in the future and compare the setups.

[^iptables]: `iptables -A FORWARD -m state --state RELATED,ESTABLISHED -j ACCEPT`
[^track_ips]: I used `1.1.1.1`, `8.8.8.8` and `9.9.9.9` but any stable IP should work.
