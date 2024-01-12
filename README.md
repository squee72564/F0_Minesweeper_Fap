


# Minesweeper Implementation for the Flipper Zero.

## Installation
1. Clone this repo to your computer ie:
	`git clone https://github.com/squee72564/F0_Minesweeper_Fap.git`
3. I would recommend using the [uFBT (micro Flipper Build Tool)](https://github.com/flipperdevices/flipperzero-ufbt) to build the application.
	- It allows you to compile into .fap files on the SD card as opposed to re-flashing the entire firmware with the application.
	- With uFBT installed you can go into the root directory of this project after cloning and type
	`ufbt launch` to compile and launch the app on your Flipper Zero
4. Profit

##Notice

This branch of the repository has the application stripped down to just the board generation algorithm and allows you to run and profile that portion of the code.

I am using this to see some of the stats on how likely it is for a solvable board to be generated when at a certain dimension, % mines, and how long it takes the algorithm on average to solve.