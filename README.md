# rubiks_cube
A rubik's cube that runs in your terminal!

## Compilation:

### Depedencies: ncurses or curses

Use
```
make
```
or
```
gcc 3d.c CAM-curses-ascii-matcher/CAM.c -Wall -Ofast -ffast-math -lm -lncurses -o rubiks_cube
```
to compile on Linux.

On Windows, compilation can be done using a port of curses for Windows such as [pdcurses](https://pdcurses.org/).

## How to Use:
Start the application in a terminal. Make sure that the terminal is the correct size before starting the application, as you cannot resize the terminal during runtime as of now.
### Controls
- Press Left, Right, Up, Down, z, or Z to orient the cube
  - Left and Right rotate the cube around the y axis.
  - Up and Down rotate the cube around the x axis.
  - z and Z rotate the cube around the z axis.
- Press f/F, b/B, l/L, r/R, u/U, or d/D to turn a face
  - f is forward, b back, l left, r right, u up, d down. Pressing Shift-face, ie F, B, L, R, U, or D will turn the face clockwise instead of counterclockwise.
- Press q to quit

### Feedback is welcome!

### Possible upcoming features
- The ability to use any nxn cube
- A scramble button
- An automatic, efficient solver
