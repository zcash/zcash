source "$stdenv/setup"

# Set ZC_TRACE=1 in a derivation to see clean xtrace.
if [ -n "${ZC_TRACE:-}" ]
then
  function zc-build-util-cleanup {
    local ecode=$?;
    set +x
    return $ecode;
  }

  trap 'zc-build-util-cleanup; exitHandler' EXIT
  set -x
fi

set -euo pipefail

