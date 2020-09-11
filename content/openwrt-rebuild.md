---
title: "OpenWrt Rebuild"
date: 2019-11-24T22:50:47-10:00
draft: true
---

After some work on the reproducibility of OpenWrt [lynxis][lynxis-blog]
motivated me to write down what it did. This is my very first technical blog
post and therefore may receive any number of additional edits in the future.
I'll cover my [`rebuild`][rebuild] setup and also what commits I suggested to
upstream OpenWrt to make increase reproducibility.

First of all, if you never heard of reproducible builds and don't know why it's
desirable, please check out the [Reproducible Builds project website][r-b]. It
covers the basics, a great newsletter and links to other projects efforts to
make them reproducible.

For the sake of reading (and writing) I'll now abbreviate *reproducible-builds*
with *r-b*.

My interest in *r-b* begun at the [2019 OpenWrt meeting in
Hamburg][hamburg2019] (I'm the guy on the very left). I had the chance to meet
[Holger][holger] who gave a quick presentation of *r-b* and what steps OpenWrt
could do. 

## Introducing `buildinfo` files

A first step to make things reproducible is to store all relevant build
information, what tools where used during compiling etc. As OpenWrt ships it's
own toolchain, only the commit hash is needed to to recreate the very same
toolchain used for a previous build. The key word is [buildinfo
file][buildinfofiles], as used by Debian and Archlinux.

To end up with the same toolchain and source code, you have to store just three information:

### `version.buildinfo`

The [`openwrt.git`][openwrtgit] commit hash, the content could look like this:

	r11582-b3779e920e

### `config.buildinfo` (former `config.seed`)

This file contains all settings for the build, what target and profile to
build, which packages, which extra `busybox` options. For the [official
builds][openwrtdownloads] this file is rather short as it sticks mostly to the
defaults:

	CONFIG_TARGET_x86=y
	CONFIG_TARGET_x86_64=y
	CONFIG_TARGET_x86_64_Generic=y
	CONFIG_ALL_KMODS=y
	CONFIG_ALL_NONSHARED=y
	CONFIG_DEVEL=y
	CONFIG_AUTOREMOVE=y
	CONFIG_BUILDBOT=y
	CONFIG_COLLECT_KERNEL_DEBUG=y
	CONFIG_IB=y
	CONFIG_JSON_ADD_IMAGE_INFO=y
	CONFIG_KERNEL_BUILD_DOMAIN="buildhost"
	CONFIG_KERNEL_BUILD_USER="builder"
	CONFIG_SDK=y

### `feeds.buildinfo`

The *feeds* are external sources used to install packages and targets which are
not part of `openwrt.git`. As the feeds are usually Git repositories the
versioning happens here via a commit hash attached to the URL:

	src-git packages https://git.openwrt.org/feed/packages.git^cf73e1b
	src-git luci https://git.openwrt.org/project/luci.git^4d76194
	src-git routing https://git.openwrt.org/feed/routing.git^3140794
	src-git telephony https://git.openwrt.org/feed/telephony.git^53d89aa

While the creation of these files is trivial, these information weren't stored
by default. As a first step I extended the build setup:

	454021581f build: add buildinfo files for reproducibility
	6caf437652 build: add buildinfo as single Makefile target

And with a little back and forth also extended the [buildbot][buildbot]:

	1bf7b47 phase1: use buildinfo instead of prepare
	cfd7203 phase1: run prepare instead of diffconfig

As a result, all snapshots and also the upcoming **19.07** release will offer
those files! Know we know what was used to build, time to rebuild it and
compare things.

## Rebuilding

	 _______                     ________        __
	|       |.-----.-----.-----.|  |  |  |.----.|  |_
	|   -   ||  _  |  -__|     ||  |  |  ||   _||   _|
	|_______||   __|_____|__|__||________||__|  |____|
	         |__| W I R E L E S S   F R E E D O M
	-----------------------------------------------------
	OpenWrt rebuild, 2019-11-17
	-----------------------------------------------------

The here described `rebuild` script is inspired by the *r-b* script
[`reproducible_openwrt.sh`][r-b-openwrt]. The *r-b* script builds OpenWrt twice
on different servers and modifies various environmental variables, however does
not change the build path and or user, neither actually rebuilds the official
images offered at openwrt.org. That motivated me to rework the script.

Currently all builds on *r-b* are managed by a [Jenkins][debian-jenkins]
instance. As I'm currently working with GitLab CI I decided to create a
[`gitlab-ci.yml`][gitlab-ci] instead. However as most work is done within the
`rebuild` script the same would work within jenkins as well.

The idea is pretty simple. All official OpenWrt images are created via the
[buildbot][buildbot], so all that's to be done is do the very same steps. Turns
out the [actual buildbot][buildbot-phase1] setup is rather complex, at least on
a first glimps. Instead of spinning up my own buildbot instance I decided to
filter out all relevant steps, rebuild images via a Python script, compare the
outcome and offer a [simple overview page][rebuild].

I'm not sure how much to go into details here, so if you require any additional information please don't hesitate to ask and I'll extend this post!

### The steps

So everything the script does by default is shown below. I'll then split up the
sections and go into more detail.

{{< highlight python >}}
if __name__ == "__main__":
    clone_git()
    setup_buildinfo()
    checkout_commit()
    get_commit_log()
    setup_key()
    update_feeds()
    make("tools/tar/compile")
    make("tools/install")
    make("toolchain/install")
    make("target/compile")
    make("package/compile")
    make("package/install")
    make("package/index", "CONFIG_SIGNED_PACKAGES=")
    if version == "SNAPSHOT":
        add_kmods_feed()
    make("target/install")
    make("buildinfo", "V=s")
    make("checksum", "V=s")
    reset_target_output()
    compare_checksums()
    calculate_repro_stats()
    render_website()
    diffoscope_multithread()
    copy_unreproducible()
    print(context)
    print(list(rebuild_dir.glob("*")))
{{< / highlight >}}

#### Setup the source

Every build is started from zero. Unlike the buildbots there is no caching of
anything, no tools, no toolchain, not even downloaded source files - something
I should certainly improve in the near future. The first few function therefore
clones `openwrt.git`, downloads the buildinfo files, setup the `.config` file,
`feeds.conf` and lastly checks out the commit hash found. 

At this point all sources are the same state as they've been for the buildbot
when building. So ideally, once build everything, the very same images should
appear. And with very same it's not only about the same functionality the
actual checksums. More on the actual situation in a bit.

#### Actually building things

The next steps are rather boring and consuming the most time. After one another
various `make` calls are executed. After some 40 minutes on a 8 core server,
actual images are creates.

#### Comparing the output

The following function downloads the official checksums and returns a list
containing the file name and checksum.

{{< highlight python >}}
def parse_origin_sha256sums():
    sha256sums = get_file(download_url + "/sha256sums")
    return re.findall(r"(.+?) \*(.+?)\n", sha256sums)

{{< / highlight >}}

Each file found in the list is checked for existence and if found hashed and
compared to the checksum in the list. Missing files and checksum mismatches are
stored in two individual lists.

While it's useful to know which files are mismatching, it is even better to
know what lead to the different checksums. However here comes a little
challenge: As OpenWrt images mostly use [SquashFS][squashfs] to compress the
root file system, a tool like `diff` would only output a lengthy stream of
gibberish. The squashed firmware images therefore have to be unpackaged first.
However there is more! As the firmware contains a filesystem, not only the file
contents but also properties like owner and modification date are important.
As this is a common problem in the *r-b* world, a great tool called
[`diffoscope`][diffoscope] ([online version][trydiffoscope]) offers a convenient
way to compare even complex files, containing compressions and binary data. At
it's base it allows to compare two files, however unless tools like `diff` it
automatically unpacks all archives and compressions it finds and also tries to
convert all binary files to a human readable format. To serve as a website,
`diffoscope`s output is stored as HTML and linked in the text step of the
`rebuild` script.

#### Creating the website

![rebuild ath79](/posts/images/rebuild-ath79.png)

The previously mentioned *r-b* script creates a [HTML overview][r-b-overview]
containing the state of which files are reproducible which not. Instead of
using their shell scripts Holger suggested to use [mustache][mustache], a
simple template engine for various programming languages. To make the website
look a bit more appealing I copied the CSS files from downloads.openwrt.org and
modified a bit the table structure.

![rebuild ath79](/posts/images/rebuild-overview.png)

The overview uses a little caching trick. Not only snapshots are rebuild daily,
but also the current *19.07* snapshot builds. All rendered HTML files are
cached between builds and as a last step CI put together via the [`merge.py`
script][merge]. Without the caching only a single version would be available per build.

#### Extending the `gitlab-ci.yml`

Curently only four targets and two versions (snapshots and 19.07) are tested,
however the `gitlab-ci.yml` is easily extendable and the `merge` script from
above automatically shows added targets/versions.

	.rebuild:
	  variables:
	    OUTPUT_DIR: "$CI_PROJECT_DIR/output/"
	  image:
	    name: aparcar/rebuild-diffoscope
	    entrypoint: [""]
	  stage: build
	  script:
	    - python3 rebuild.py
	  artifacts:
	    paths:
	      - output
	    expire_in: 1 week
	  only:
	    - master
	  before_script:
	    - export TARGET="$(echo ${CI_JOB_NAME} | cut -d '_' -f 2 | tr '-' '/')"

	rebuild_ath79-generic: { extends: .rebuild }
	rebuild_x86-64: { extends: .rebuild }
	rebuild_ramips-mt7621: { extends: .rebuild }
	rebuild_ramips-mt7620: { extends: .rebuild }

While this setup works quite good, the caching trick breaks once multiple
workers are used, as they don't share the same cache. Someone has to come up
with a cleaner solution for that.

#### `context.json`

For the rendering via `mustache` but also to make things machine readable, a
[context file][context] is created after every build. Ideally this file would
be stored in a database so graphs show the reproduciblity over time. 

![rebuild context](/posts/images/rebuild-context.png)







[lynxis-blog]: https://lunarius.fe80.eu/blog/
[rebuild]: https://rebuild.aparcar.org/
[r-b]: https://reproducible-builds.org/
[hamburg2019]: https://openwrt.org/meetings/hamburg2019/
[holger]: https://wiki.debian.org/HolgerLevsen
[buildinfofiles]: https://wiki.debian.org/ReproducibleBuilds/BuildinfoFiles
[openwrtgit]: https://git.openwrt.org/?p=openwrt/openwrt.git;a=summary
[buildinfo-pr]: https://github.com/openwrt/openwrt/pull/2121
[buildbot]: http://buildbot.openwrt.org/master/images/
[buildbot-phase1]: https://git.openwrt.org/?p=buildbot.git;a=blob;f=phase1/master.cfg;h=792f9b3a55a2d8ebae1917c56716cc0d699f9172;hb=HEAD
[diffoscope]: https://salsa.debian.org/reproducible-builds/diffoscope
[trydiffoscope]: http://try.diffoscope.org/
[squashfs]: https://en.wikipedia.org/wiki/SquashFS
[r-b-openwrt]: https://salsa.debian.org/qa/jenkins.debian.net/blob/c469abb34687746d83f88acccdca31c9fb3e3ecf/bin/reproducible_openwrt.sh
[r-b-overview]: https://tests.reproducible-builds.org/openwrt/openwrt_ar71xx.html
[mustache]: http://mustache.github.io/
[debian-jenkins]: https://jenkins.debian.net/
[gitlab-ci]: https://gitlab.com/aparcar/rebuild/blob/master/.gitlab-ci.yml
[merge]: https://gitlab.com/aparcar/rebuild/blob/master/merge.py
[context]: https://rebuild.aparcar.org/SNAPSHOT/x86/64/context.json
