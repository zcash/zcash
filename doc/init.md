Sample systemd service configuration for zcashd
===============================================

Sample script and configuration file for systemd:

    contrib/init/zcashd.service:    systemd service unit configuration

Service User
------------

This systemd configuration assumes the existence of a "zcash" user
and group.  They must be created before attempting to use these scripts.

Configuration
-------------

You must also run zcash-fetch-params (or `zcutil/fetch-params.sh` from source)
as this user, and create a zcash.conf in your `/home/zcash/.zcash/` directory.

For an example configuration file that describes the configuration settings,
see `contrib/debian/examples/zcash.conf`.

Paths
-----

This configuration assumes several paths:

**Binary:**              /usr/bin/zcashd

**Configuration file:**  /home/zcash/.zcash/zcash.conf

**Data directory:**      /home/zcash/.zcash

The configuration file and data directory should all be owned by the zcash
user and group.  It is advised for security reasons to make the configuration
file and data directory only readable by the zcash user and group.  Access to
zcash-cli and other zcashd RPC clients can then be controlled by group membership.

Installing Service Configuration
--------------------------------

Installing this .service file consists of just copying it to
`/etc/systemd/system/` directory, followed by the command
"systemctl daemon-reload" in order to update running systemd configuration.

To test, run "systemctl start zcashd" and to enable for system startup run
"systemctl enable zcashd"
