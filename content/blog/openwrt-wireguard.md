---
title: "OpenWrt with Wireguard VPN"
date: 2021-10-26
draft: false
---

There are [many][many1] [many][many2] [many][many3] tutorials on how to setup
Wireguard VPN on Debian (Ubuntu) and OpenWrt, however I want to keep it here for
my personal notes.  This setup describes a *network address traversal* (NAT)
tunnel server as well as a *pinging* client. The client can connect to the
Internet using the tunnel servers IP and the tunnel server can login to a client
since it *pings* the tunnel server with its address and open port.

## Setup tunnel server

While the tunnel server can also be a OpenWrt device, this setup describes using
a Debian (Ubuntu) server. First install `wireguard-tools` since we'll need the
`wg-quick` tool in a moment.

```shell
apt update
apt install wireguard-tools
```

Enable IPv4 forwarding with the following command:

```shell
sysctl -w net.ipv4.ip_forward=1
```

To be persistent this option should be uncommented/added to `/etc/sysctl.conf`.

On Debian each Wireguard interface is defined in a single configuration file
stored in `/etc/wireguard/`. A common approach is to call the configuration file
for the first interface `wg0.conf`. The configuration contains an interface
definition and any number of peers. 

The interface section contains the used VPN IP address including subnet size as
well as a port. The port will be used by connecting clients. 
```ini
# /etc/wireguard/wg0.conf
[Interface]
Address = 10.0.0.1/24
ListenPort = 51820
PostUp = iptables -A FORWARD -i %i -j ACCEPT; iptables -t nat -A POSTROUTING -o enp2s0 -j MASQUERADE
PostDown = iptables -D FORWARD -i %i -j ACCEPT; iptables -t nat -D POSTROUTING -o enp2s0 -j MASQUERADE
```

The `Address` is important since it defines the number of clients that can
connect. This demo setup uses a `/24` network while real setups should use much
bigger subnets.

The `PostUp` and `PostDown` commands are run by the `wg-quick` command to enable
*NAT* for the interface. The port is flexible and just have to be publicly
reachable.

The Wireguard protocol is designed to stay silent if the incoming packets are
not valid. Valid means the packet comes from a known peer and is encrypted with
its private key. If the package is not valid Wireguard will not react at all,
making the port seem closed. However if a firewall is in place the selected
`Port` must be opened.

Next a private key is added, assuming the `wg0.conf` file only contains the
options shown above, the following command will add a private key:

```shell
echo "PrivateKey = $(wg genkey)" >> /etc/wireguard/wg0.conf
```

To enable the interface the `wg-quick` command is used as shown below:

```shell
wg-quick up wg0
```

Running the `wg` command should now present the interface including the public
key. The public key should be copied since it's needed in the next step, where a
client is setup.

```shell
interface: wg0
  public key: 25ljRJgjoxxxxxxxxxxxxxxxxxxxx6/01xV4f7GB8=
  private key: (hidden)
  listening port: 51820
```

In the next section a client is setup and the clients public key plus IP address
will then be added to the tunnel server Wireguard configuration.

## Add a client (aka peer)

The following commands are run on a device running OpenWrt. Install the
`wireguard-tools` since the `wg` tool is required to setup the Wireguard
protocols.

```shell
opkg update
opkg install wireguard-tools
```

Next export env variables containing the tunnel servers public key, it's
endpoint including port and the devices IP address. The device IP address should
not yet exists on the server.

```shell
export SERVER_WG_PUBKEY='25ljRJgjoxxxxxxxxxxxxxxxxxxxx61xV4f7GB8='
export ENDPOINT_HOST="148.xxx.xxx.xxx"
export ENDPOINT_PORT="51820"
export DEVICE_IP="10.0.0.3/24"
```

Once the variables are set a new interface called `wg0` is added to the network
configuration. The commands automatically create a private key.

```shell
uci set network.wg0=interface
uci set network.wg0.proto='wireguard'
uci set network.wg0.private_key="$(wg genkey)"
uci set network.wg0.addresses="$DEVICE_IP"
uci set network.wg0.proto='wireguard'

uci set network.tunnelserver=wireguard_wg0
uci set network.tunnelserver.description='tunnel-server'
uci set network.tunnelserver.allowed_ips='0.0.0.0/0'
uci set network.tunnelserver.endpoint_host="$ENDPOINT_HOST"
uci set network.tunnelserver.endpoint_port="$ENDPOINT_PORT"
uci set network.tunnelserver.persistent_keepalive=25
uci set network.tunnelserver.public_key="$SERVER_WG_PUBKEY"
uci set network.tunnelserver.route_allowed_ips='true'

uci commit network
```

After restarting the network via `service network restart` the new Wireguard
interface should be available. To see if it worked run the `wg` command.

> Restarting the network will route all traffic but the tunnel IP connection
> over the Wireguard interface. Since the setup is not yet done, connectivity is
> lost until the tunnel server configuration is updated. If you're setting up a
> remote device the `route_allowed_ips` should be set to `false` until both ends
> are setup.

```shell
interface: wg0
  public key: NgvbLeF4cVSxxxxxxxxS84R7wdzlXzs=
  private key: (hidden)
  listening port: 54969

peer: 25ljRJgjo7xyxxxxxxxxxxxxxx/01xV4f7GB8=
  endpoint: 148.xxx.xxx.xxx:51xxx
  allowed ips: 0.0.0.0/0
  transfer: 0 B received, 148 B sent
  persistent keepalive: every 25 seconds

```

## Add new peer to tunnel server

The tunnel server doesn't know about the new peer yet, so it has to be added to
the peers network configuration. This is done by adding three lines to the
`/etc/wireguard/wg0.conf` file. The following lines should be added:

```shell
[Peer]
PublicKey = <peer_public_key>
AllowedIPs = <peer_ip_address>
```

The `PublicKey` and `AllowedIPs` should be copied from the client in the last
step. Once added a configuration reload happens via the following command:

```shell
wg syncconf wg0 <(wg-quick strip wg0)
```

Running `wg` on the tunnel server should show an existing established connection
since the client will try to ping the tunnel server every 25 seconds.

```shell
interface: wg0
  public key: 25ljRJgjoxxxxxxxxxxxxxxxxxxxx/01xV4f7GB8=
  private key: (hidden)
  listening port: 51820

peer: NgvbLeF4cVSxTxxxxxxxxxxxxx84R7wdzlXzs=
  endpoint: 168.xxx.xxx.xxx:55142
  allowed ips: 10.0.0.3/32
  latest handshake: 6 seconds ago
  transfer: 17.87 KiB received, 29.40 KiB sent
```

A ping command like `ping 10.0.0.3` on the tunnel server should show a working
connection.

```shell
ping 10.0.0.03 -c3
PING 10.0.0.03 (10.0.0.3) 56(84) bytes of data.
64 bytes from 10.0.0.3: icmp_seq=1 ttl=64 time=207 ms
64 bytes from 10.0.0.3: icmp_seq=2 ttl=64 time=229 ms
64 bytes from 10.0.0.3: icmp_seq=3 ttl=64 time=252 ms
```

From now on the client router will route all it's traffic over the tunnel
server. To disable this behaviour it is possible to set the *allowed IPs* for
the tunnel server to a specific IP instead.

```shell
uci set network.tunnelserver.allowed_ips='10.0.0.1/24'
```

This way not all traffic but only the tunnel server specific traffic is
forwarded.

[many1]: http://chrisbuchan.co.uk/computing/wireguard-setup-openwrt/
[many2]: https://openwrt.org/docs/guide-user/services/vpn/wireguard/server
[many3]: https://casept.github.io/post/wireguard-server-on-openwrt-router/
