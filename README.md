Links
=====
Links is a Lynx-like text and graphics WWW browser.

This fork cleans up a lot of legacy code but removes the graphics mode.
If that mode was important to you then steer clear.

WARNING: This has been subtly broken for a while. It sometimes segfaults on
unexpected pages in a way that is difficult to debug (it forces gdb to the
background on segfault and `fg` just sends it back to the background). I still
occasionally use it for reading mailing list archives but fixing it is not high
priority. Patches are always welcome!

Requirements
------------
In order to build links you need the following:
* libevent
* libssl


Installation
---------------
Edit config.mk to match your local setup (links is installed into
the /usr/local namespace by default).

Afterwards enter the following command to build and install links (if
necessary as root):
```
make clean install
```
