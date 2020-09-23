#!/bin/bash
set -efuxo pipefail

# cd to the repo root:
cd "$(readlink -f "$0/../../../")"
docker build -t zcash-dev-docker ./contrib/dev-docker/
exec docker run -v "$(pwd)/:/workspace" zcash-dev-docker "$@"
