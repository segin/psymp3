#!/bin/bash
set -e
git checkout master
git pull

# Extract IDs from prs.txt, reverse the order so the oldest (bottom of list) is merged first
ids=$(awk '{print $1}' prs.txt | tac)

for id in $ids; do
  echo "Merging PR $id..."
  gh pr merge $id --merge --delete-branch || {
    echo "Failed to merge PR $id. Stopping."
    exit 1
  }
  echo "Successfully merged PR $id."
done
echo "All done merging."
