#!/bin/sh
# Prepares the DAV root and lock directory, then runs httpd in the foreground.
set -e
mkdir -p /usr/local/apache2/dav /usr/local/apache2/var
chown daemon:daemon /usr/local/apache2/dav /usr/local/apache2/var
htpasswd -bc /usr/local/apache2/conf/htpasswd rb rbpass
exec httpd -f /usr/local/apache2/conf/rb-httpd.conf -DFOREGROUND
