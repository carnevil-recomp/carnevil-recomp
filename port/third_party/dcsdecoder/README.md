# DCS Decoder (vendored)

Michael J Roberts' native C++ re-implementation of the DCS audio decoder, from
https://github.com/mjrgh/DCSExplorer (BSD 3-clause, see the headers), taken from the copy in
https://github.com/tinktinp/img-viewer (packages/dcs-decoder, commit 623108e, which carries the
author's patches for the Midway video-game variants).

Only the decoder sources are vendored; the port drives them from `port/host/dcs_board.cpp`,
which plays the role of CarnEvil's DCS2 sound board: it takes the words the game's sound
library writes to the I/O ASIC sound port, keeps the uploaded banks in a DRAM image, runs the
bank track programs, and uses this decoder as the stream codec.

Fixes to the decoder itself should go upstream to DCSExplorer.
