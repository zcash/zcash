#!/bin/bash

for f in $(git diff --name-only --diff-filter=U | cat); do
   echo "Resolve conflict in $f ..."
   git checkout --theirs $f
done

for f in $(git diff --name-only --diff-filter=U | cat); do
   echo "Adding file $f ..."
   git add $f
done

