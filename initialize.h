//
// Created by lauren on 1/25/18.
//

#ifndef LEMONSOUFFLE_INITIALIZE_H
#define LEMONSOUFFLE_INITIALIZE_H

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <map>
#include "bead.h"           //stores particle class (gary)
#include "edge.h"
#include "unit.h"
#include "face.h"
#include "box.h"
#include "LJpair.h"
#include "oligomer.h"
#include "vector3d.h"           //stores VECTOR3D class

void initialize_system(std::vector<BEAD> & rgary,std::vector<EDGE> & rgedge,std::vector<UNIT> & rgarfield, \
                        std::vector<FACE> & rgface, VECTOR3D bxsz, BOX & rtardis, std::vector<PAIR> &rgpair);

void initialize_outputfile(std::ofstream & reftraj, std::ofstream & refofile);

void allonsy (double capconc,unsigned int numden, std::string file_name);

void initialize_bead_velocities(std::vector<UNIT> &garfield, std::vector<BEAD> &gary, double T);


#endif //LEMONSOUFFLE_INITIALIZE_H