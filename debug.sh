#!/bin/bash
rm out
pgrep streaner

echo "logging to out"
./streamer -v 1 &> out


