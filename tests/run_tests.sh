#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

TESTS=(
    test_ringbuf
    test_diskio
)

make "${TESTS[@]}" 2>&1

for t in "${TESTS[@]}"; do
    echo "--- $t ---"
    ./"$t"
done