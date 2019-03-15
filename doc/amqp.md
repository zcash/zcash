# Block and Transaction Broadcasting With AMQP 1.0 (Experimental Feature)

[AMQP](https://www.amqp.org/) is an enterprise-level message queuing
protocol for the reliable passing of real-time data and business
transactions between applications.  AMQP supports both broker and
brokerless messaging.  AMQP 1.0 is an open standard and has been
ratified as ISO/IEC 19464.

The Zcash daemon can be configured to act as a trusted "border
router", implementing the Zcash P2P protocol and relay, making
consensus decisions, maintaining the local blockchain database,
broadcasting locally generated transactions into the network, and
providing a queryable RPC interface to interact on a polled basis for
requesting blockchain related data. However, there exists only a
limited service to notify external software of events like the arrival
of new blocks or transactions.

The AMQP facility implements a notification interface through a set
of specific notifiers. Currently there are notifiers that publish
blocks and transactions. This read-only facility requires only the
connection of a corresponding AMQP subscriber port in receiving
software.

Currently the facility is not authenticated nor is there any two-way
protocol involvement. Therefore, subscribers should validate the
received data since it may be out of date, incomplete or even invalid.

Because AMQP is message oriented, subscribers receive transactions
and blocks all-at-once and do not need to implement any sort of
buffering or reassembly.

## Prerequisites

The AMQP feature in Zcash requires [Qpid Proton](https://qpid.apache.org/proton/)
version 0.17 or newer, which you will need to install if you are not
using the depends system. Typically, it is packaged by distributions as
something like *libqpid-proton*. The C++ wrapper for AMQP *is* required.

In order to run the example Python client scripts in contrib/ one must
also install *python-qpid-proton*, though this is not necessary for
daemon operation.

## Security WARNING

Enabling this feature even on the loopback interface only (e.g. binding
it to localhost or 127.0.0.1) will still expose it to the wilds of the
Internet, because of an attack vector called DNS rebinding. DNS
rebinding allows an attacker located remotely on the Internet to trick
applications that you're running on the same computer as Zcashd to
contact your supposedly localhost-only AMQP port, then, depending on the
program they may be able to attempt to attack it.

Do not enable this feature unless you are sure that you know what you
are doing, and that you have a strong reason for thinking that you are
not vulnerable to this type of attack.

## Enabling

By default, the AMQP feature is automatically compiled in if the
necessary prerequisites are found.  To disable, use --disable-proton
during the *configure* step of building zcashd:

    $ ./configure --disable-proton (other options)

To actually enable operation, one must set the appropriate options on
the commandline or in the configuration file.

## Usage

AMQP support is currently an experimental feature, so you must pass
the option:

    -experimentalfeatures

Currently, the following notifications are supported:

    -amqppubhashtx=address
    -amqppubhashblock=address
    -amqppubrawblock=address
    -amqppubrawtx=address

The address must be a valid AMQP address, where the same address can be
used in more than notification.  Note that SSL and SASL addresses are
not currently supported.

Launch zcashd like this:

    $ zcashd -amqppubhashtx=amqp://127.0.0.1:5672

Or this:

    $ zcashd -amqppubhashtx=amqp://127.0.0.1:5672 \
        -amqppubrawtx=amqp://127.0.0.1:5672 \
        -amqppubrawblock=amqp://127.0.0.1:5672 \
        -amqppubhashblock=amqp://127.0.0.1:5672 \
        -debug=amqp

The debug category `amqp` enables AMQP-related logging.

Each notification has a topic and body, where the header corresponds
to the notification type. For instance, for the notification `-amqpubhashtx`
the topic is `hashtx` (no null terminator) and the body is the hexadecimal
transaction hash (32 bytes).  This transaction hash and the block hash
found in `hashblock` are in RPC byte order.

These options can also be provided in zcash.conf.

Please see `contrib/amqp/amqp_sub.py` for a working example of an
AMQP server listening for messages.

## Remarks

From the perspective of zcashd, the local end of an AMQP link is write-only.

No information is broadcast that wasn't already received from the public
P2P network.

No authentication or authorization is done on peers that zcashd connects
to; it is assumed that the AMQP link is exposed only to trusted entities,
using other means such as firewalling.

TLS support may be added once OpenSSL has been removed from the Zcash
project and alternative TLS implementations have been evaluated.

SASL support may be added in a future update for secure communication.

Note that when the block chain tip changes, a reorganisation may occur
and just the tip will be notified. It is up to the subscriber to
retrieve the chain from the last known block to the new tip.

At present, zcashd does not try to resend a notification if there was
a problem confirming receipt.  Support for delivery guarantees such as
*at-least-once* and *exactly-once* will be added in in a future update.

Currently, zcashd appends an up-counting sequence number to each notification
which allows listeners to detect lost notifications.

