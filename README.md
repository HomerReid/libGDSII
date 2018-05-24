libGDSII is a C++ library for working with GDSII binary data files, intended primarily for use with the computational electromagnetism codes
[scuff-em](http://homerreid.github.io/scuff-em-documentation)
and
[meep](http://meep.readthedocs.org)
but sufficiently general-purpose to allow other uses as well.

The packages consists of

+ a C++ library (`libGDSII`)  with API functions for reading, processing, and exporting GDSII files

+ a command-line executable code (`GDSIIConvert`) for reporting statistics on GDSII geometries and
  exporting them to other file formats, notably including the [GMSH](gmsh.info) geometry format.

--------------------------------------------------
# Installation
--------------------------------------------------

```bash
% git clone http://github.com/HomerReid/libGDSII
% cd libGDSII
% sh autogen.sh --prefix=/path/to/installation/prefix
% make install
```

This will install the `GDSIIConvert` executable in `$(prefix)/bin`
and the `libGDSII.a` and/or `libGDSII.so` library binaries in $(prefix)/lib`.

--------------------------------------------------
# Sample usage of `GDSIIConvert` command-line tool
--------------------------------------------------

The GDSII file referenced in the following examples is 
[this silicon-photonics transceiver](https://github.com/lukasc-ubc/SiEPIC-Tools/blob/master/Examples/GSiP/RingModTransceiver/Layouts/GSiP_4_RingFilter.gds)
from the [SiEPIC-Tools](https://github.com/lukasc-ubc) project:

+ Lukas Chrostowski, Zeqin Lu, Jonas Flueckiger, Xu Wang, Jackson Klein, 
  Amy Liu, Jaspreet Jhoja, James Pond, 
  ["Design and simulation of silicon photonic schematics and layouts,"](http://edx.org/course/silicon-photonics-design-fabrication-ubcx-phot1x)
  Proc. SPIE 9891, Silicon Photonics and Photonic Integrated Circuits V, 989114 (May 13, 2016); doi:10.1117/12.2230376.
```

## Convert to [GMSH](http://gmsh.info) format

```bash
% GDSIIConvert --GMSH GSiP_4_RingFilter.gds

Read 2080 data records from file GSiP_4_RingFilter.gds.
Wrote 91 text strings to GSiP_4_RingFilter.pp.
Detecting metallization structures on layer   1: ... 34 polygons on layer 1
Detecting metallization structures on layer   3: ... 30 polygons on layer 3
Detecting metallization structures on layer   7: ... 2 polygons on layer 7
Detecting metallization structures on layer  10: ... 78 polygons on layer 10
Detecting metallization structures on layer  20: ... 1 polygons on layer 20
Detecting metallization structures on layer  21: ... 1 polygons on layer 21
Detecting metallization structures on layer  44: ... 12 polygons on layer 44
Detecting metallization structures on layer  47: ... 30 polygons on layer 47
Detecting metallization structures on layer  60: ... 2 polygons on layer 60
Detecting metallization structures on layer  63: ... 2 polygons on layer 63
Detecting metallization structures on layer  66: ... 87 polygons on layer 66
Detecting metallization structures on layer  68: ... 16 polygons on layer 68
Detecting metallization structures on layer  69: ... 46 polygons on layer 69
Detecting metallization structures on layer  81: ... 2 polygons on layer 81
Detecting metallization structures on layer 733: ... 2 polygons on layer 733
Wrote 345 metallization polygons to GSiP_4_RingFilter.geo.
Thank you for your support.
```bash

This produces two files: the GMSH geometry file `GSiP_4_RingFilter.geo,`
describing the polygons, and the GMSH post-processing file `GSiP_4_RingFilter.pp`
describing the text strings. These can be opened in GMSH for visualization:

```bash
% gmsh GSiP_4_RingFilter.geo GSiP_4_RingFilter.pp
```

![GMSH screenshot](GMSHScreenshot.png)

### Extracting individual layers

If you only want to extract polygons on a single layer, you can 
add e.g. `--MetalLayer 47` to the `GDSIIConvert` command line:

```bash
% GDSIIConvert --GMSH --MetalLayer 47 GSiP_4_RingFilter.geo --FileBase GSIP_47.geo
% GDSIIConvert --GMSH --MetalLayer 69 GSiP_4_RingFilter.geo --FileBase GSIP_69.geo
```

## Print text description of geometry hierarchy:

```bash

% GDSIIConvert --analyze GSiP_4_RingFilter.gds

Read 2080 data records from file GSiP_4_RingFilter.gds.
*
* File GSiP_4_RingFilter.gds:
* Unit=1.000000e-06 meters (file units = {1.000000e-03,1.000000e-09})
*
**************************************************
** Library SiEPIC_GSiP:
**************************************************
--------------------------------------------------
** Struct 0: $$$CONTEXT_INFO$$$
--------------------------------------------------
  Element 0: SREF (layer 0, datatype 0)
    (structure CIRCLE)
    (attribute 7: PCELL_CIRCLE)
    (attribute 6: P_actual_radius)
    (attribute 5: P_npoints)
    (attribute 4: P_handle___dpoint)
    (attribute 3: P_radius)
    (attribute 2: P_layer___layer)
    (attribute 1: LIB_Basic)
    (attribute 0: LIB_SiEPIC_GSiP)
     XY: 0 0

  Element 1: SREF (layer 0, datatype 0)
    (structure OpticalFibre__micron)
    (attribute 1: CELL_OpticalFibre__micron)
    (attribute 0: LIB_SiEPIC_GSiP)
     XY: 0 0

  Element 2: SREF (layer 0, datatype 0)
    (structure ROUND_PATH$)

...
...
...
  Element 18: SREF (layer 0, datatype 0)
    (structure ROUND_PATH$)
     XY: 0 0

  Element 19: SREF (layer 0, datatype 0)
    (structure ROUND_PATH$)
     XY: 0 0

  Element 20: BOUNDARY (layer 20, datatype 0)
     XY: 228898 187725 228803 187791 228898 187931 228898 187725

  Element 21: BOUNDARY (layer 21, datatype 0)
     XY: 234248 192138 234248 192595 234250 192366 234248 192138

Thank you for your support.
```

## Print low-level description of GDSII file structure:

```bash

% GDSIIConvert --raw GSiP_4_RingFilter.gds

Record 0:       HEADER ( 1)  = 600
Record 1:       BGNLIB ( 12)  = 2017 3 16 23 29 47 2017 3 16 23 29 47
Record 2:      LIBNAME ( 1)  = SiEPIC_GSiP
Record 3:        UNITS ( 2)  = 0.001 1e-09
Record 4:       BGNSTR ( 12)  = 2017 3 16 23 29 47 2017 3 16 23 29 47
Record 5:      STRNAME ( 1)  = $$$CONTEXT_INFO$$$
Record 6:         SREF =
Record 7:        SNAME ( 1)  = CIRCLE
Record 8:           XY ( 2)  = 0 0
Record 9:     PROPATTR ( 1)  = 7
Record 10:    PROPVALUE ( 1)  = PCELL_CIRCLE
Record 11:     PROPATTR ( 1)  = 6
Record 12:    PROPVALUE ( 1)  = P_actual_radius
...
...
...
Record 2066:           XY ( 2)  = 0 0
Record 2067:        ENDEL =
Record 2068:     BOUNDARY =
Record 2069:        LAYER ( 1)  = 20
Record 2070:     DATATYPE ( 1)  = 0
Record 2071:           XY ( 8)  = 228898 187725 228803 187791 228898 187931 228898 187725
Record 2072:        ENDEL =
Record 2073:     BOUNDARY =
Record 2074:        LAYER ( 1)  = 21
Record 2075:     DATATYPE ( 1)  = 0
Record 2076:           XY ( 8)  = 234248 192138 234248 192595 234250 192366 234248 192138
Record 2077:        ENDEL =
Record 2078:       ENDSTR =
Record 2079:       ENDLIB =
Read 2080 data records from file SiEPIC/GSiP_4_RingFilter.gds.
Thank you for your support. 
```

--------------------------------------------------
# Sample usage of `libGDSII` API routines
--------------------------------------------------

`libGDSII` exports a C++ class called `GDSIIData,` whose
class constructor accepts the name of a binary GDSII file as input.

Internally, the constructor maintains both a *hierarchical* representation
of the geometry (involving structures that instantiate other structures,
arrays of structure elements, etc.) and a *flat* representation,
consisting simply of collections of polygons and text labels,
indexed by the layer on which they appeared.

The flat representation is simply a huge list of *entities*,
where each entity is either a *polygon*  or *text label*
plus a layer index indicating the layer of the GDSII geometry 
on which the entity lies.

In the flat representation, all GDSII geometry elements---including
boundaries, boxes, paths, structure instantiations (`SREF`s), and
arrays (`AREF`s)---are reduced to polygons, described by lists
of $N>1$ vertices, with each vertex having two coordinates (*x* and *y*).
Thus the data of a polygon consists of an integer $N$ (number of vertices),
$2N$ floating-point numbers (vertex coordinates), and an integer layer index.
The $2N$ vertex coordinates are stored internally as a 

The only other type of entity is a text label. The data of a text label
consists of a character string (the label), two floating-point numbers
(*x,y*) specifying the *location* of the label (typically the
centroid of the text), and a layer index.

`GDSIIData` provides API routines for extracting polygons from a geometry
by 


```C++

#include "libGDSII.h"
using namespace std;
using namespace libGDSII;

  /********************************************************************/
  /* try to instantiate GDSIIData structure from binary GDSII file    */
  /********************************************************************/
  GDSIIData *gdsIIData  = new GDSIIData( string("GSiP_4_RingFilter.gds") );

  if (gdsIIData->ErrMsg)
   { printf("error: %s (aborting)\n",gdsIIData->ErrMsg->c_str());
     exit(1);
   }

  /***************************************************************/
  /* output a text-based description of the geometry             */
  /***************************************************************/
  gdsIIData->WriteDescription();                // writes to console
  gdsIIData->WriteDescription("MyOutputFile");

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/

```
