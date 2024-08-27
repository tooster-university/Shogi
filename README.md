# Shogi app

This is a C++ implementation of the Shogi game for linux. It consists of a GUI app for playing Shogi on a hot-seat computer and a simple engine implementing standard Shogi rules.

## Functionality

- hot-seat playthrough with GUI
- fully implemented rules engine
- ability to save and load game
- time tracking ([see Known issues](README.md#known-issues))
- History view showing moves in the standard Shogi notation
- rules are available in the "Help" menu 

## Dependencies

- cmake
- gtk3

## Installation and running

1. clone the repository, for example `git clone https://github.com/tooster-university/Shogi.git`
2. go into directory `cd Shogi`
3. run CMake: `cmake .`
4. build the project: `make`
5. run the project: `./shogi`

## Known issues

- Player timers can be inaccurate.
