#!/bin/bash

if [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
    echo "Running outside of pull request isn't supported yet"
    exit 0
fi

# Choose the commit against which to format
base_commit=$(git rev-parse $TRAVIS_BRANCH)
echo "Formatting against $TRAVIS_BRANCH a.k.a. $base_commit..."
echo

scripts/lint_clang_format.sh $1 $base_commit
