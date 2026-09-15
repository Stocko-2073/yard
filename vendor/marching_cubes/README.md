# Marching-cubes lookup tables

Source: https://github.com/pmneila/PyMCubes/blob/649095092a629acdd1d9588d44d589ab7a9cae5b/mcubes/src/marchingcubes.cpp

Revision: `649095092a629acdd1d9588d44d589ab7a9cae5b`.
Only `edge_table` and `triangle_table` are copied. Their declarations use
`static const unsigned short` and `static const signed char` for C11;
the table values are unchanged. See the accompanying BSD-3-Clause license.
Normal builds do not fetch dependencies.
