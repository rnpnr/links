Links
=====
Links is a Lynx-like text and graphics WWW browser.

This fork cleans up a lot of legacy code but removes the graphics mode.
If that mode was important to you then steer clear.


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
