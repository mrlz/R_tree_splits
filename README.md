### What is this repository for? ###
C implementation of R-trees comparing Quadratic split against Linear split. 

Main function contains a while that creates test data (using create_rectangles function), inserts it to the tree and then begins to search.

### How do I get set up? ###
Compile with (GCC 4.8.4):
gcc t.c
gcc t_linear.c

t.c contains Quadratic split implementation.
t_linear.c contains Linear split implementation.

Made under Ubuntu 14.04.3.