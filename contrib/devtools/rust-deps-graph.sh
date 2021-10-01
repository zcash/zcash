#!/usr/bin/env bash

export LC_ALL=C

cargo deps --no-transitive-deps | dot \
    -Earrowhead=vee \
    -Gratio=0.45 \
    -Gsize=50 \
    -Nheight=0.8 \
    -Nfillcolor=LavenderBlush \
    -Nstyle=filled \
    -Nfontsize=18 \
    -Nmargin=0.05,0.05 \
    -Tpng > rust-dependency-graph.png
