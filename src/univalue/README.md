
# UniValue

## Summary

A universal value class, with JSON encoding and decoding.

UniValue is an abstract data type that may be a null, boolean, string,
number, array container, or a key/value dictionary container, nested to
an arbitrary depth.

This class is aligned with the JSON standard, [RFC
7159](https://tools.ietf.org/html/rfc7159.html).

## Motivation

UniValue is a reaction to json_spirit, seeking to minimize template
and memory use, providing a straightforward RAII class compatible with
link-time optimization and embedded uses.

## Status

The current production version is available from the [stable-1.0.x branch](https://github.com/jgarzik/univalue/tree/stable-1.0.x).

The current development series is 1.1.x, and is pushed to the `master` branch.

The next stable version will be 1.2.0, to be released immediately
following the conclusion of the 1.1.x series, similar to [this
variant](https://en.wikipedia.org/wiki/Software_versioning#Odd-numbered_versions_for_development_releases)
of semver.

## Installation

This project is a standard GNU
[autotools](https://www.gnu.org/software/automake/manual/html_node/Autotools-Introduction.html)
project.  Build and install instructions are available in the `INSTALL`
file provided with GNU autotools.

```
$ ./autogen.sh
$ ./configure
$ make
```

