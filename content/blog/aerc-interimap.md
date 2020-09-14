---
title: "Aerc with Interimap"
date: 2020-09-13T22:24:45-10:00
draft: false
---
While using a mixture of *Thunderbird* and *Evolution* since the beginning of
(my) time, I'm recurrently tempted by seemingly faster alternatives running
inside the terminal. After trying *Mutt* with no long term success due to it's
complex configuration, I stumbled multiple times over [aerc], which was
initially started by [Drew DeVault][drew], a developer I like for his
[many][srht] [great][sway] [projects][git-mail].

Until recently I tested *aerc* either via [offlineimap] and a local *maildir*
or via a pure *IMAP* connection. The former makes E-Mail readable without an
active Internet connection, however folders of big mailing lists slow down
*aerc* and *offlineimap* doesn't sync very fast overall. The *IMAP* option
works amazing, except for the missing offline feature, which comes in very
handy at times.

Luckily a friend let me know about [interimap], which works similar to
*offlineimap*, but uses a local [dovecot] instance to handle E-Mail storage and
is **[much faster]**.
The setup is fairly simple and well described on the authors
[website][interimap-start]. It describes how to use *interimap* with 
*dovecot* via a wrapper script, so no permanently running instance of *dovecot*
is required. Instead of connecting on `localhost:993` it uses `doveadm` which
executes `imap`.

The sync process works as fast as excepted and within a few minutes I had all
my emails locally available. In the last step the *interimap* author describes
a setup with *Mutt*, while I'd like to use *aerc* for its simplicity.

## Configuration

Using *aerc* with the *interimap* wrapper script wasn't as easy as I executed.
*Mutt* does support a `tunnel` command which allows to run a script before
connecting to a *IMAP* server. As for now *aerc*, which uses internally
[go-imap], doesn't offer this feature. Using the maildir of *dovecot* wasn't a
real alternative for me, as it would slow down *aerc* again on long lists.

So I looked into the code of *go-imap* to add a tunnel feature, just to realize
that `socat` does exactly what I need. Instead of modifying any code, I just
connect the wrapper script to port `localhost:10993`:

```shell
socat TCP-LISTEN:10993,reuseaddr,pf=ip4,fork exec:/home/user/.local/bin/dovecot-imap
```

In the *aerc* configuration I use a *insecure IMAP* connection:

```shell
source = imap+insecure://localhost:10993
```

That's all it takes to have an extremely responsive and offline available
*aerc* setup!

## Multiple accounts

It becomes slightly trickier with multiple accounts. The wrapper script and
*dovecot* both use prefixes for mailboxes (RFC3501 6.3.8), meaning a single
connection contains multiple email accounts. By filtering single mail accounts
are connected as *view*. This feature is also [implemented in
go-imap][rfc3501], however not yet supported in *aerc*.

Instead of using the prefixes, I used the `folder` option with a trivial RegEx:

```shell
# accounts.conf
[foobar]
folder = ~foobar/*
default = foobar/INBOX
copy-to = foobar/Sent

[barfoo]
folder = ~barfoo/*
default = barfoo/INBOX
copy-to = barfoo/Sent
```

This way only folders prefixed with the selected account are shown, even though
both mail accounts contain the exact same email. A cleaner solution would be to
request only correctly prefixed folders from *dovecot* instead of hiding
non-matching folder names, but it works for now. All other folders like
`default` and `copy-to` have to be manually prefixed with the same prefix
chosen for *interimap* and *dovecot*.

## Cosmetic workaround

With the configuration described above, all folders listed are prefixed with
the account name, which is a waste of space. Until real `prefix` handling is
implemented in *aerc* I added patched the `dirlist` formatting so that the
first 8 characters are removed, coincidently the length of all my accounts.

```diff
diff --git a/widgets/dirlist.go b/widgets/dirlist.go
index aca1491..9617a80 100644
--- a/widgets/dirlist.go
+++ b/widgets/dirlist.go
@@ -148,7 +148,7 @@ func (dirlist *DirectoryList) getDirString(name string, width int, recentUnseen
                                        doRightJustify(name)
                                        rightJustify = false
                                }
-                               formatted += name
+                               formatted += name[8:]
                                percent = false
                        }
                case 'r':
```

This is clearly a very subjective patch. I'll try to get more familiar with the
codebase and maybe propose a patch implementing real prefix handling.

[much faster]: https://guilhem.org/interimap/benchmark.html
[aerc]: https://git.sr.ht/~sircmpwn/aerc
[drew]: https://drewdevault.com/
[srht]: https://sourcehut.org/
[sway]: https://swaywm.org/
[git-mail]: https://git-send-email.io/
[offlineimap]: https://www.offlineimap.org/
[interimap]: https://guilhem.org/interimap/
[dovecot]: https://www.dovecot.org/
[interimap-start]: https://guilhem.org/interimap/getting-started.html
[go-imap]: https://github.com/emersion/go-imap/
[rfc3501]: https://github.com/emersion/go-imap/blob/3bc38e360e3c75d52e3e2c60f3597369154ec4b4/mailbox.go#L136
