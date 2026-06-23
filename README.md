# CartoCrow: Chorematic Map
[![DOI](https://zenodo.org/badge/1128254673.svg)](https://doi.org/10.5281/zenodo.18155179)

The chorematic_map module implements an algorithm for summarizing classed region maps with a single disk choreme.
The algorithm is described in the following paper.
> Steven van den Broek, Wouter Meulemans, Andreas Reimer, and Bettina Speckmann. Summarizing classed region maps with a disk choreme. To appear in the International Journal of Cartography. 

## Dependencies
This module depends on the following libraries:

* [Cartocrow/core](https://github.com/cartocrow/core) (v0.1.0)

CartoCrow/core can be cloned and built using CMake as described in <https://github.com/cartocrow/core>

## Usage

The program expects two types of data: geometries and statistical attributes.
These can be loaded in different ways; the `data` folder contains some example data files.
Both geometries and statistical attributes can be loaded from a `.gpkg` file, of which two example files are given (they are also available and citable via Zenodo: https://zenodo.org/records/15524996).
Geometries can also be loaded from `.ipe` files (https://ipe.otfried.org/), and statistical attributes can also be loaded from `.csv` files.

Run `chorematic_map_gui` in the root directory or use a JSON file with the command-line interface.
For example, to create a chorematic diagram of the Netherlands that shows the average distance to the nearest pharmacy, run the following command in the root directory.
```
chorematic_map_cl data/chorematic_map-dutch.json output.svg data/gemeenten-2022_5000vtcs.ipe
```
This creates an `output.svg` file containing the diagram.

## License

Copyright (c) 2026 TU Eindhoven
Licensed under the GPLv3.0 license. See LICENSE for details.
