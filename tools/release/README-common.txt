CarnEvil (Midway, 1998) - native port from the carnevil-decomp decompilation project
https://github.com/carnevil-recomp/carnevil-recomp

This package contains NO game data. You need your own dump of the CarnEvil v1.0.3 hard
disk (MAME set "carnevil", carnevil.chd), turned into a raw image with MAME's chdman:

    chdman extractraw -i carnevil.chd -o carnevil.raw

The first run unpacks the game's files from the image into ./state/partition (once).
Settings, credits and high scores are kept in ./state (delete it for a factory-fresh boot).
