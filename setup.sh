#!/bin/sh
# Links the original game files and the SDK into the repo (nothing is copied or committed).
set -e
cd "$(dirname "$0")"
mkdir -p original
ln -sfn "${CARNEVIL_PARTITION:-$HOME/Downloads/carnevil-chd/out/_extracted_partition}" original/partition
ln -sfn "${CARNEVIL_RAW:-$HOME/Downloads/carnevil-chd/carn.raw}" original/carn.raw
ln -sfn "${MIDWAY_SDK:-$HOME/Downloads/carnevil-chd/midwayvideo}" original/sdk
ln -sfn "${CARNEVIL_ROMS:-$HOME/Downloads/carnevil}" original/roms
echo "linked:"; ls -l original
