#!/bin/sh
# Links the original game files and the SDK into the repo (nothing is copied or committed).
set -e
cd "$(dirname "$0")"
mkdir -p original
# Target is CarnEvil v1.0.3 (MAME parent set "carnevil"). v1.0.1 (MAME "carnevil1", BAD_DUMP) is kept for reference.
# Re-create the partition dirs from the CHDs with:
#   chdman extractraw -i carnevil.chd -o carnevil_103.raw && tools/lfn32.py extract carnevil_103.raw@1 v103/partition
ln -sfn "${CARNEVIL_PARTITION:-$HOME/Downloads/carnevil-chd/v103/partition}" original/partition
ln -sfn "${CARNEVIL_RAW:-$HOME/Downloads/carnevil-chd/carnevil_103.raw}" original/carn.raw
ln -sfn "${CARNEVIL_PARTITION_101:-$HOME/Downloads/carnevil-chd/v101/partition}" original/partition_v101
ln -sfn "${CARNEVIL_RAW_101:-$HOME/Downloads/carnevil-chd/carn.raw}" original/carn_v101.raw
ln -sfn "${MIDWAY_SDK:-$HOME/Downloads/carnevil-chd/midwayvideo}" original/sdk
ln -sfn "${CARNEVIL_ROMS:-$HOME/Downloads/carnevil}" original/roms
echo "linked:"; ls -l original
