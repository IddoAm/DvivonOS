#!/bin/bash

set -e

# fetch the latest changes from the origin remote
git fetch origin

# push all branches to the gitlab remote
git push gitlab --prune --all
git push gitlab --prune --tags