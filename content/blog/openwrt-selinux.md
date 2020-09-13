+++
title = "Running OpenWrt with SELinux"
date = 2020-09-11T11:04:01-10:00
draft = false
tags = []
categories = []
+++ 
This blog post describes the creation and testing of OpenWrt images running
SELinux to improve security. The support is still limited and not ready for any
productive usage, however this post should give a basic idea of its current
state.

## History 

Back in November 2019 Thomas Petazzoni sent a [first patchset] adding optional
SELinux support to OpenWrt. Due to the complexity of integrating SELinux into
the OpenWrt build system and missing bits in the patchset, the patches never
made it into the main branch.

In July 2020 W. Michael Petullo gave the patchset a [second spin], updated
legacy Python2 to Python3, rebased everything and reworked all change requests
from the reviewers. Special thanks to Daniel Golle. Due to the deep integration
of SELinux inside the operating system special versions of `busybox`, `procd`
and `f2fs-tools` [were created][variants], keeping the regular (non SELinux)
images small while allowing to integrate the additional SELinux feature via
package installations.

## Creating images

To try SELinux on OpenWrt it is currently required to compile the Kernel with
additional options. There are ideas to provide dual Kernel ImageBuilders with a
regular and a SELinux ready version, however this isn't ready just yet.

You'll need a OpenWrt [build system] and a bit of time!

As of 2020/09 only `x86/64`, `armvirt/64` and `ath79/generc` using a *squashfs*
filesystem where tested with SELinux, if you use a different target, please
share your results.

The following build options are required:

```shell
Global build settings ->
	# CONFIG_TARGET_ROOTFS_SECURITY_LABELS=y
	[*] Enable rootfs security labels

	Kernel build options ->
		# CONFIG_KERNEL_SECURITY_SELINUX=y
		[*] NSA SELinux Support
```

These options will automatically select the package `refpolicy` to set labels
inside the created `squashfs` filesystem.

Select the SELinux variants of `procd`, `busybox` and (optionally)
`mkf2fs`. Be sure to deselect the *regular* versions as there is currently
a dependency error if both variants are selected.

```shell
Base system ->
	<*> busybox-selinux
	<*> procd-selinux

Utilities ->
	Filesystem ->
		# optional: only if target uses f2fs
		<*> mkf2fs-selinux			
```

## Running

The following will run an `armvirt/64` image:

```shell
BIN_DIR=./bin/targets/armvirt/64

qemu-system-aarch64 \ 
        -M virt \
        -append "console=ttyAMA0,115200 root=/dev/vda" \
        -cpu cortex-a57 \
        -device virtio-blk-device,drive=hd \
        -drive file="$BIN_DIR/openwrt-armvirt-64-rootfs-squashfs.img",if=none,format=raw,id=hd \
        -kernel "$BIN_DIR/openwrt-armvirt-64-Image" \
        -nographic 
```

A boot log is written directly to the used terminal and should show lines like the following:

```shell
[    0.002483] LSM: Security Framework initializing
[    0.005608] SELinux:  Initializing.
...
[    2.732347] SELinux:  policy capability network_peer_controls=1
[    2.732662] SELinux:  policy capability open_perms=1
[    2.732773] SELinux:  policy capability extended_socket_class=1
[    2.732899] SELinux:  policy capability always_check_network=0
[    2.733022] SELinux:  policy capability cgroup_seclabel=1
[    2.733132] SELinux:  policy capability nnp_nosuid_transition=1
[    2.753174] audit: type=1403 audit(1599863280.884:2): auid=4294967295 ses=4294967295 lsm=selinux res=1
```

If `refpolicy` worked all files should received an initial label:

```shell
root@OpenWrt:/# ls -Z
system_u:object_r:bin_t          bin
system_u:object_r:tmpfs_t        dev
system_u:object_r:etc_t          etc
system_u:object_r:default_t      init
system_u:object_r:lib_t          lib
system_u:object_r:lib_t          lib64
system_u:object_r:mnt_t          mnt
system_u:object_r:unlabeled_t    overlay
system_u:object_r:proc_t         proc
system_u:object_r:root_t         rom
root:object_r:user_home_dir_t    root
system_u:object_r:bin_t          sbin
system_u:object_r:sysfs_t        sys
system_u:object_r:tmpfs_t        tmp
system_u:object_r:usr_t          usr
system_u:object_r:default_t      var
system_u:object_r:default_t      www
```

Also processes will receive a label, however `procd` does not yet set contexts.
This involves some C programming and should be tackled in the near future. The
list below show that `dropbear`, `netifd` and `odhcpd` all share the same
context labels.

```shell
root@OpenWrt:/# ps -Z
  PID CONTEXT                          STAT COMMAND
    1 system_u:system_r:init_t         S    /sbin/procd
    2 system_u:system_r:kernel_t       SW   [kthreadd]
    3 system_u:system_r:kernel_t       IW<  [rcu_gp]
    4 system_u:system_r:kernel_t       IW<  [rcu_par_gp]
    5 system_u:system_r:kernel_t       IW   [kworker/0:0-eve]
    6 system_u:system_r:kernel_t       IW<  [kworker/0:0H-kb]
    7 system_u:system_r:kernel_t       IW   [kworker/u2:0-ev]
...
  985 system_u:system_r:init_t         S    /usr/sbin/dropbear -F -P /var/run/
 1060 system_u:system_r:init_t         S    /sbin/netifd
 1100 system_u:system_r:init_t         S    /usr/sbin/odhcpd
 1327 system_u:system_r:init_t         S<   /usr/sbin/ntpd -n -N -S /usr/sbin/
 1712 system_u:system_r:init_t         R    ps -Z
```

This wraps up the current state. There is still more to do, a few points in the
next section.

## ToDo

The fun didn't quite come to an end yet:

* ~~A [pending Pull Request] over at `openwrt/packages.git` will add additional
  tools like `audit2allow`.~~ Merged
* Extending  [`procd`][procd] to set correct contexts
* Use `refpolicy` labels in [`ipkg-build`][ipkg-build].
* Offer ImageBuilders with a second Kernel supporting SELinux features.

[first patchset]: https://lists.infradead.org/pipermail/openwrt-devel/2019-November/025973.html
[second spin]: https://github.com/openwrt/openwrt/pull/3207
[variants]: https://github.com/openwrt/openwrt/pull/3303
[build system]: https://openwrt.org/docs/guide-developer/quickstart-build-images
[pending Pull Request]: https://github.com/openwrt/packages/pull/10664
[procd]: https://git.openwrt.org/?p=project/procd.git
[ipkg-build]: https://github.com/openwrt/openwrt/blob/master/scripts/ipkg-build
