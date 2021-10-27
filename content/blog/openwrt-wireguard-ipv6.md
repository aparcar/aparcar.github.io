---
title: "OpenWrt with Wireguard VPN (IPv6)"
date: 2021-10-27
draft: false
---

> This is a follow up on the previous post on [how to set up
> Wireguard]({{< ref "./openwrt-wireguard.md" >}}).

In case IPv6 traffic on OpenWrt clients should be handled as well, this post
describes how to distribute a IPv6 subnet to clients of the tunnel server.

The setup assumes that the tunnel server uses an IPv6 subnet that is big enough
to split into multiple smaller networks. In this setup the tunnel server has a
`/64` subnet and distributes `/80` networks to clients.

To calculate subnets I recommend the excellent OpenWrt
[`owipcalc`](https://github.com/openwrt/packages/tree/master/net/owipcalc).
However other calculators should do the trick as well.

```shell
owipcalc ff:ff:ff:ff::2/64 howmany ::/80

> 65536
```

For this setup no more than 65536 will be available, but that's just enough.
Adjust the subnets based on your tunnel server address space and needs.

## Extend tunnel server configuration

If the IPv4 already works, only minor changes are required. Since this setup
distributes an existing IPv6 address, it does not need to be added to the
wireguard interface on the tunnel server. However, the `AllowedIPs` peer section
needs to be extended to contain the public client address. The command below
calculates the next `/80` network which is assigned to the client.

```shell
owipcalc ff:ff:ff:ff::2/64 next ::/80

> ff:ff:ff:ff:1::2/80
```

Extend the configuration in `/etc/wireguard/wg0.conf` for the selected client:

```ini
AllowedIPs = 10.0.0.3/32,ff:ff:ff:ff:1::/80
```

> Wireguard prints a warning if you don't remove the trailing `2` since it's
> expecting a subnet. Leaving the `2` won't cause any issues as it's
> automatically removed.

Reload the configuration with the command below and very the settings are
applied by running `wg`.

```shell
wg syncconf wg0 <(wg-quick strip wg0)
wg

> [...]
> peer: NgvbLeF4cVSxTxxxxxxxxxxxxx84R7wdzlXzs=
>   endpoint: 168.xxx.xxx.xxx:55142
>   allowed ips: 10.0.0.3/32, ff:ff:ff:ff:1::/80
>   latest handshake: 6 seconds ago
>   transfer: 17.87 KiB received, 29.40 KiB sent
```

## Extending OpenWrt client configuration

The configuration of the end device is trivial, all it takes is adding another
`addresses` element to the list.

```shell
uci add_list network.wg0.adresses='ff:ff:ff:ff:1::2/80'
uci commit network
```

To verify, the content of `/etc/config/network` should look similar to the
following:

```ini
config interface 'wg0'
        option proto 'wireguard'
        list addresses '10.0.0.3/24'
        list addresses 'ff:ff:ff:ff:1::2/80'
        option private_key 'KI70xxxxxoxxxxxxxxxxxxxxxxxxxxxxxxvmgFc='
```

Restarting the network via `service network restart` enabled the IPv6 and allows
now IPv6 traffic.

Test your newly set IPv6 address via the following command:

```shell
uclient-fetch http://ipv6.ident.me -q -O -

> ff:ff:ff:ff:1::2
```

On the other side it's possible to log into your client remotely via the IPv6
address.

### Client Firewall

Per default the `wg0` Wireguard interface isn't assigned to a firewall zone.
This means whoever knows the IPv6 address of the client can both login via SSH
and login to LuCI (if installed). Adding `wg0` to the `WAN` interface solves
this issue, however prevents remote logins, if desired.

For my setup I only allow SSH logins via public keys and run LuCI (if installed)
on `localhost` only. These decision however depend on the users setup.

```shell
# /etc/config/uhttpd
config uhttpd 'main'
        list listen_http '127.0.0.1:80'
        list listen_http '[::1]:80'
        list listen_https '127.0.0.1:443'
        list listen_https '[::1]:443'
[...]

# /etc/config/dropbear
config dropbear
	option PasswordAuth 'off'
	option RootPasswordAuth 'off'
	option Port '22'
```
