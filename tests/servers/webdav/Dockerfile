# WebDAV server for the protocol e2e tests (scripts/dev.sh servers).
FROM httpd:2.4-alpine
# mod_dav's lock database needs a DBM driver; the base image ships none.
RUN apk add --no-cache apr-util-dbm_gdbm
COPY httpd.conf /usr/local/apache2/conf/rb-httpd.conf
COPY entry.sh /usr/local/bin/rb-entry.sh
CMD ["/bin/sh", "/usr/local/bin/rb-entry.sh"]
