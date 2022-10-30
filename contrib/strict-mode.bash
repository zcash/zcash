#   This is an agglomerated BASH strict mode. It is intended to be sourced into
#   other scripts. I.e., any script should start with the following two lines:
#
#     #!/usr/bin/env bash
#     source ./path/to/bash-strict-mode
#
#   resources:
# • https://olivergondza.github.io/2019/10/01/bash-strict-mode.html
# • https://dougrichardson.us/notes/fail-fast-bash-scripting.html
# • https://olivergondza.github.io/2019/10/01/bash-strict-mode.html
set -euo pipefail
shopt -s inherit_errexit
trap 's=$?; echo "$0: Error on line "$LINENO": $BASH_COMMAND"; exit $s' ERR
