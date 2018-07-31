#!/bin/sh

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

KOMODOD=${KOMODOD:-$SRCDIR/komodod}
KOMODOCLI=${KOMODOCLI:-$SRCDIR/komodo-cli}
KOMODOTX=${KOMODOTX:-$SRCDIR/komodo-tx}

[ ! -x $KOMODOD ] && echo "$KOMODOD not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
KMDVER=($($KOMODOCLI --version | head -n1 | awk -F'[ -]' '{ print $5, $6 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for komodod if --version-string is not set,
# but has different outcomes for komodo-cli.
echo "[COPYRIGHT]" > footer.h2m
$KOMODOD --version | sed -n '1!p' >> footer.h2m

for cmd in $KOMODOD $KOMODOCLI $KOMODOTX; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${KMDVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${KMDVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
