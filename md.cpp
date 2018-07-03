//
// Created by lauren on 6/7/18.
//

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include "initialize.h"
#include "functions.h"
#include "md.h"

using namespace std;


void run_molecular () {

    double const vdwr = pow(2, 0.166666666667);       //System specific paramters (modelled for HBV)
    double const SImass = 6.18e-24; //kg               //SI value for a single bead
    double const SIsigma = 1.67e-9; //m
    double const SItime = 7.06e-11; //s
    double const Avagadro = 6.022e23; // mol^-1
    int filenumber = 100000;

    vector<BEAD> gary;                                      //Create particles, named gary (short for garfield)
    vector<EDGE> gedge;                                     //create bonds between gary's
    vector<UNIT> garfield;                                  //create subunits, named garfield
    vector<FACE> gface;                                     //create faces of gary's on garfield
    vector<PAIR> gpair;                                     //create vector to hold LJ pairings
    vector<THERMOSTAT> real_bath;                           //vector of thermostats
    BOX tardis;                                             //creates a simulation box

    double totaltime, ecut, qs, capconc, saltconc, numden, ks, kb, delt;
    string file_name;
    cout << "Total time:" << endl;                  cin >> totaltime;
    cout << "ecut:" << endl;                        cin >> ecut;
    cout << "Solvent valency:" << endl;             cin >> qs;
    cout << "capsomere concentration:" << endl;     cin >> capconc;
    cout << "salt concentration" << endl;           cin >> saltconc;
    cout << "number of capsomeres:" << endl;        cin >> numden;
    cout << "stretching constant" << endl;          cin >> ks;
    cout << "bending constant" << endl;             cin >> kb;
    cout << "input parameter filename:" << endl;    cin >> file_name;
    cout << "timestep" << endl;                     cin >> delt;



    double chain_length_real, Q, T;
    cout << "Nose' Hoover chain length:" << endl;   cin >> chain_length_real;
    cout << "Nose' Hoover mass" << endl;            cin >> Q;
    cout << "Desired set temperature:" << endl;     cin >> T;


    allonsy(capconc, numden, file_name);     //Setting up the input file (uses user specified file to generate lattice)

    double box_x = pow((numden * 1000 / (capconc * pow(SIsigma, 3) * 6.022e23)), 1.0 / 3.0);    //calculating box size
    VECTOR3D bxsz = VECTOR3D(box_x, box_x, box_x);

    initialize_system(gary, gedge, garfield, gface, bxsz, tardis, gpair);


                                                                        //user-derived parameters (not edittable)
    double lb = 0.416;                                // e^2 / (4 pi Er E0 Kb T)
    double ni = saltconc * Avagadro * SIsigma * SIsigma * SIsigma;       //number density (1/sigma*^3)
    int count = 0;                                    //used in oligomer counting later on...


    vector<OLIGOMER> ollie;                                 //setting up other objects used in analysis
    vector<int> massbins(garfield.size());
    vector<vector<int> > ms_bin(totaltime / (delt * 1000), vector<int>(garfield.size()));

    cout << endl << "Simulation will run for " << totaltime * SItime / (1e-9) << " nanoseconds with a "
         << delt * SItime / (1e-12) << " picosecond timestep." << endl;

    ofstream traj("gary.traj.out", ios::out);              //setting up file outputs
    ofstream ofile("ovito.lammpstrj", ios::out);
    ofstream msdata("ms.out", ios::out);
    initialize_outputfile(traj, ofile);

    /*                                                    NOSE-HOOVER CHAIN THERMOSTAT                                  */
    if (chain_length_real == 1)
        real_bath.push_back((THERMOSTAT(0, T, 3 * gary.size(), 0.0, 0, 0, 1)));
    else
    {
        real_bath.push_back((THERMOSTAT(Q, T, 3 * gary.size(), 0, 0, 0, 1)));
        while (real_bath.size() != chain_length_real - 1)
            real_bath.push_back((THERMOSTAT(Q / (3 * gary.size()), T, 1, 0, 0, 0,1)));
        real_bath.push_back((THERMOSTAT(0, T, 3 * gary.size(), 0.0, 0, 0,1)));
// final bath is dummy bath (dummy bath always has zero mass)
    }



/*                  ___                        __      __      ___
      /|   /|      |   \             |        /  \    /  \    |   \
     / |  / |      |    \            |       |    |  |    |   |    |
    /  | /  | ---- |     |           |       |    |  |    |   |___/
   /   |/   |      |    /            |       |    |  |    |   |
  /         |      |___/             |_____   \__/    \__/    |                       */

// Calculate initial forces
    dress_up(gedge, gface);

    for (int i = 0; i < gary.size(); i++)
        gary[i].update_stretching_force(ks, vdwr);

    for (int i = 0; i < gedge.size(); i++) {
        if (gedge[i].type != 0)                     //if it is a bending edge
            gedge[i].update_bending_forces(kb);
    }

    update_ES_forces(gary, lb, ni, qs);

    update_LJ_forces(gary, ecut, gpair);

    double senergy = 0;                                 //blank all the energy metrics
    double kenergy = 0;
    double benergy = 0;
    double tenergy = 0;
    double ljenergy = 0;
    double cenergy = 0;
    double tpenergy = 0;
    double tkenergy = 0;

    initialize_bead_velocities(garfield, gary, T);            //assign random velocities based on initial temperature

    double particle_ke = particle_kinetic_energy(gary);     //thermostat variables
    double expfac_real;

    int mstime = -1;                                          //parameter for ms_bin filling

    cout << endl << endl << "Calculating..." << endl << endl;

    for (unsigned int a = 0; a < (totaltime / delt); a++)         // BEGIN MD LOOP
    {
                                                                    //thermostat set up
        for (int j = real_bath.size() - 1; j > -1; j--)
            update_chain_xi(j, real_bath, delt, particle_ke);
        for (unsigned int j = 0; j < real_bath.size(); j++)
            real_bath[j].update_eta(delt);

        expfac_real = exp(-0.5 * delt * real_bath[0].xi);

        for (unsigned int i = 0; i < gary.size(); i++)
            gary[i].therm_update_velocity(delt, real_bath[0], expfac_real);  //update velocity half step

        for (unsigned int i = 0; i < gary.size(); i++)
            gary[i].update_position(delt);                 //update position full step

        dress_up(gedge, gface);                              //update edge and face properties

        for (unsigned int i = 0; i < gary.size(); i++) {
            gary[i].update_stretching_force(ks, vdwr);          //calculate forces
            gary[i].bforce = VECTOR3D(0, 0, 0);                 //blanking out bending force here
        }

        update_ES_forces(gary, lb, ni, qs);

        update_LJ_forces(gary, ecut, gpair);

        for (unsigned int i = 0; i < gedge.size(); i++) {
            if (gedge[i].type != 0)
                gedge[i].update_bending_forces(kb);
        }

        for (unsigned int i = 0; i < gary.size(); i++) {
            gary[i].therm_update_velocity(delt, real_bath[0], expfac_real);    //update velocity the other half step
        }

        particle_ke = particle_kinetic_energy(gary);
// forward update of Nose-Hoover chain
        for (unsigned int j = 0; j < real_bath.size(); j++)
            real_bath[j].update_eta(delt);
        for (unsigned int j = 0; j < real_bath.size(); j++)
            update_chain_xi(j, real_bath, delt, particle_ke);


/*      __                 __                        ____     ________     ____
 *     /  \    |\    |    /  \    |       \   /     /    \        |       /    \
 *    |____|   | \   |   |____|   |        \ /      \_            |       \_
 *    |    |   |  \  |   |    |   |         |         \__         |         \__
 *    |    |   |   \ |   |    |   |         |            \        |            \
 *    |    |   |    \|   |    |   |_____    |       \____/    ____|____   \____/                                */





        if (a % 100 == 0) {                                           //analysis loop (energies)

            for (unsigned int i = 0; i < gary.size(); i++) {
                gary[i].ne = 0;                                     //blanking out energies here (ADD TO FXN 2-8)
                gary[i].be = 0;
                gary[i].ce = 0;
            }

            update_ES_energies(gary, lb, ni, qs);

            update_LJ_energies(gary, ecut, gpair);

            for (unsigned int i = 0; i < gedge.size(); i++) {
                if (gedge[i].type != 0) {
                    gedge[i].update_bending_energy(kb);
                }
            }

            for (unsigned int i = 0; i < gary.size(); i++) {
                gary[i].update_stretching_energy(ks, vdwr);
                gary[i].update_kinetic_energy();
                senergy += gary[i].se;                          //sum up total energies
                kenergy += gary[i].ke;
                ljenergy += gary[i].ne;
                benergy += gary[i].be;
                cenergy += gary[i].ce;
            }
            for (unsigned int i = 0; i < real_bath.size(); i++) {        //thermostat energies
                real_bath[i].potential_energy();
                real_bath[i].kinetic_energy();
            }
            for (unsigned int i = 0; i < real_bath.size(); i++) {
                tpenergy += real_bath[i].pe;                    //sum up total energies
                tkenergy += real_bath[i].ke;
            }

            tenergy = senergy + kenergy + ljenergy + benergy + cenergy + tpenergy +
                      tkenergy;      //print info to files for data analysis


            ofile << "ITEM: TIMESTEP" << endl << a << endl << "ITEM: NUMBER OF ATOMS" << endl << gary.size() << endl
                  << "ITEM: BOX BOUNDS" << endl << -bxsz.x / 2 << setw(15) << bxsz.x / 2 << endl << -bxsz.y / 2
                  << setw(15)
                  << bxsz.y / 2 << endl << -bxsz.z / 2 << setw(15) \
 << bxsz.z / 2 << endl << "ITEM: ATOMS index type x y z b charge" << endl;

            traj << a * delt << setw(15) << kenergy / gary.size() << setw(15) << senergy / gary.size() << setw(15) <<
                 benergy / gary.size() << setw(15) << ljenergy / gary.size() << setw(15) << cenergy / gary.size()
                 << setw(15) << tenergy / gary.size()
                 << setw(15) << (benergy + senergy + ljenergy + cenergy) / gary.size() << setw(15)
                 << kenergy * 2 / (3 * gary.size()) << setw(15) << tpenergy / gary.size() << setw(15)
                 << tkenergy / gary.size() << endl;

            for (unsigned int b = 0; b < gary.size(); b++) {
                ofile << b + 1 << setw(15) << gary[b].type << setw(15) << gary[b].pos.x << setw(15) << gary[b].pos.y \
 << setw(15) << gary[b].pos.z << setw(15) << gary[b].be << setw(15) << gary[b].q << endl;

                count += 1;

            }
            senergy = 0;                            //blanking out energies
            kenergy = 0;
            benergy = 0;
            tenergy = 0;
            ljenergy = 0;
            cenergy = 0;
            tpenergy = 0;
            tkenergy = 0;
        }


        if (a % 1000 == 0) {
//            if (a > 100000) {
//                char filename[100];                                     //pair-correlation fxn file generation
//                filenumber += 1000;
//                sprintf(filename, "data.coords.all.%d", filenumber);
//                ofstream pairout(filename, ios::out);
//                for (int i = 0; i < gary.size(); i++) {
//                    if (gary[i].type == 3) {
//                        pairout << gary[i].id << setw(15) << gary[i].type << setw(15) << gary[i].m << setw(15)
//                                << gary[i].pos.x
//                                << setw(15) << gary[i].pos.y << setw(15) << gary[i].pos.z << endl;
//                    }
//                }
//            }

            int index = -1;

            for (unsigned int i = 0; i < garfield.size(); i++)           //Create oligomers for mass spectrum analysis
            {
                double oldsize = 0;

                if (garfield[i].itsO.size() == 0) {                       //if the unit isn't already counted...

                    ollie.push_back(OLIGOMER(VECTOR3D(0, 0, 0)));     //create an oligomer for the unit
                    index += 1;
                    ollie[index].itsU.push_back(&garfield[i]);      //add unit to oligomer
                    ollie[index].id = index;
                    garfield[i].itsO.push_back(ollie[index]);               //add oligomer to unit
                    while (oldsize < ollie[index].itsU.size()) {    //while the oligomer is still growing...
                        int n = oldsize;
                        oldsize = ollie[index].itsU.size();           //see how much the oligomer has grown
                        for (int j = n; j < oldsize; j++) {             //loop over the growth from last round
                            int g = ollie[index].itsU[j]->id;
                            for (int k = i + 1; k < garfield.size(); k++) { //look for new growth
                                if (garfield[k].itsO.size() == 0) {  //if it isn't in an oligomer yet...
                                    for (int m = 0;
                                         m < garfield[g].itsB.size(); m++) { //check to see if it is in this oligomer
                                        for (int n = 0; n < garfield[k].itsB.size(); n++) {
                                            if (dist(garfield[g].itsB[m], garfield[k].itsB[n]).GetMagnitude() < 1.5) {
                                                ollie[index].itsU.push_back(&garfield[k]);   //if it is attached, add it
                                                garfield[k].itsO.push_back(ollie[index]);   //mark subunit as bonded
                                                goto finish;
                                            }
                                        }
                                    }


                                }
                                finish:;
                            }
                        }

                    }
                }
            }
            mstime += 1;
            for (int i = 0; i < ollie.size(); i++) {
                if (ollie[i].itsU.size() >= 1) {
                    ms_bin[mstime][(ollie[i].itsU.size() - 1)] += 1;          //fill mass bins
                }
            }

            for (int j = 0; j < ms_bin[mstime].size(); j++) {
                msdata << ms_bin[mstime][j] << setw(15);                //print mass bin data to file
            }
            msdata << endl;

            for (int i = 0; i < garfield.size(); i++) {             // clear oligomer pointers from subunit
                garfield[i].itsO.clear();
            }

            ollie.erase(ollie.begin(),ollie.end());                 //erases oligomer objects
// g(r) dump data


        }//end of energy analysis loop


        double fraction_completed = ((a + 1) / (totaltime / delt));   //progress bar
        ProgressBar(fraction_completed);
    } //time loop end



    compute_MD_trust_factor_R(1);                   //computes R


}




void run_brownian(){

    double const vdwr = pow(2, 0.166666666667);       //System specific paramters (modelled for HBV)
    double const SImass = 6.18e-24; //kg               //SI value for a single bead
    double const SIsigma = 1.67e-9; //m
    double const SItime = 7.06e-11; //s
    double const Avagadro = 6.022e23; // mol^-1
    int filenumber = 100000;

    vector<BEAD> gary;                                      //Create particles, named gary (short for garfield)
    vector<EDGE> gedge;                                     //create bonds between gary's
    vector<UNIT> garfield;                                  //create subunits, named garfield
    vector<FACE> gface;                                     //create faces of gary's on garfield
    vector<PAIR> gpair;                                     //create vector to hold LJ pairings
    BOX tardis;                                             //creates a simulation box

    double totaltime, ecut, qs, capconc, saltconc, numden, ks, kb, delt, T, fric_zeta;
    string file_name;
    cout << "Total time:" << endl;                  cin >> totaltime;
    cout << "ecut:" << endl;                        cin >> ecut;
    cout << "Solvent valency:" << endl;             cin >> qs;
    cout << "capsomere concentration:" << endl;     cin >> capconc;
    cout << "salt concentration" << endl;           cin >> saltconc;
    cout << "number of capsomeres:" << endl;        cin >> numden;
    cout << "stretching constant" << endl;          cin >> ks;
    cout << "bending constant" << endl;             cin >> kb;
    cout << "input parameter filename:" << endl;    cin >> file_name;
    cout << "timestep" << endl;                     cin >> delt;
    cout << "desired set temperature:" << endl;     cin >> T;
    cout << "friction coefficient:" << endl;        cin >> fric_zeta;



    allonsy(capconc, numden, file_name);     //Setting up the input file (uses user specified file to generate lattice)

    double box_x = pow((numden * 1000 / (capconc * pow(SIsigma, 3) * 6.022e23)), 1.0 / 3.0);    //calculating box size
    VECTOR3D bxsz = VECTOR3D(box_x, box_x, box_x);

    initialize_system(gary, gedge, garfield, gface, bxsz, tardis, gpair);


    //user-derived parameters (not edittable)
    double lb = 0.416;                                // e^2 / (4 pi Er E0 Kb T)
    double ni = saltconc * Avagadro * SIsigma * SIsigma * SIsigma;       //number density (1/sigma*^3)
    int count = 0;                                    //used in oligomer counting later on...


    vector<OLIGOMER> ollie;                                 //setting up other objects used in analysis
    vector<int> massbins(garfield.size());
    vector<vector<int> > ms_bin(totaltime / (delt * 1000), vector<int>(garfield.size()));

    cout << endl << "Simulation will run for " << totaltime * SItime / (1e-9) << " nanoseconds with a "
         << delt * SItime / (1e-12) << " picosecond timestep." << endl;

    ofstream traj("gary.traj.out", ios::out);              //setting up file outputs
    ofstream ofile("ovito.lammpstrj", ios::out);
    ofstream msdata("ms.out", ios::out);
    initialize_outputfile(traj, ofile);



/*                  ___                        __      __      ___
      /|   /|      |   \             |        /  \    /  \    |   \
     / |  / |      |    \            |       |    |  |    |   |    |
    /  | /  | ---- |     |           |       |    |  |    |   |___/
   /   |/   |      |    /            |       |    |  |    |   |
  /         |      |___/             |_____   \__/    \__/    |                       */

// Calculate initial forces
    dress_up(gedge, gface);

    for (int i = 0; i < gary.size(); i++)
        gary[i].update_stretching_force(ks, vdwr);

    for (int i = 0; i < gedge.size(); i++) {
        if (gedge[i].type != 0)                     //if it is a bending edge
            gedge[i].update_bending_forces(kb);
    }

    update_ES_forces(gary, lb, ni, qs);

    update_LJ_forces(gary, ecut, gpair);

    double senergy = 0;                                 //blank all the energy metrics
    double kenergy = 0;
    double benergy = 0;
    double tenergy = 0;
    double ljenergy = 0;
    double cenergy = 0;

    initialize_bead_velocities(garfield, gary, T);            //assign random velocities based on initial temperature

    int mstime = -1;                                          //parameter for ms_bin filling

    cout << endl << endl << "Calculating..." << endl << endl;

    gsl_rng * r = gsl_rng_alloc(gsl_rng_mt19937);
    unsigned long int Seed = 23410981;
    gsl_rng_set(r,Seed);

    for (unsigned int a = 0; a < (totaltime / delt); a++)         // BEGIN MD LOOP
    {


        for (unsigned int i = 0; i < gary.size(); i++) {
            //gary[i].brownian_update_velocity(delt, fric_zeta);  //update velocity half step
            gary[i].tforce = gary[i].sforce + gary[i].bforce + gary[i].ljforce + gary[i].eforce;
            gary[i].vel.x +=
                    (gary[i].vel.x * (-0.5 * fric_zeta * delt)) + (gary[i].tforce.x * (0.5 * delt / gary[i].m)) +
                    sqrt(2 * 6 * delt * fric_zeta / gary[i].m) * (gsl_rng_uniform(r) - 0.5);
            gary[i].vel.y +=
                    (gary[i].vel.y * (-0.5 * fric_zeta * delt)) + (gary[i].tforce.y * (0.5 * delt / gary[i].m)) +
                    sqrt(2 * 6 * delt * fric_zeta / gary[i].m) * (gsl_rng_uniform(r) - 0.5);
            gary[i].vel.z +=
                    (gary[i].vel.z * (-0.5 * fric_zeta * delt)) + (gary[i].tforce.z * (0.5 * delt / gary[i].m)) +
                    sqrt(2 * 6 * delt * fric_zeta / gary[i].m) * (gsl_rng_uniform(r) - 0.5);
        }

        for (unsigned int i = 0; i < gary.size(); i++)
            gary[i].update_position(delt);                 //update position full step

        dress_up(gedge, gface);                              //update edge and face properties

        for (unsigned int i = 0; i < gary.size(); i++) {
            gary[i].update_stretching_force(ks, vdwr);          //calculate forces
            gary[i].bforce = VECTOR3D(0, 0, 0);                 //blanking out bending force here
        }

        update_ES_forces(gary, lb, ni, qs);

        update_LJ_forces(gary, ecut, gpair);

        for (unsigned int i = 0; i < gedge.size(); i++) {
            if (gedge[i].type != 0)
                gedge[i].update_bending_forces(kb);
        }

        for (unsigned int i = 0; i < gary.size(); i++) {
            //gary[i].brownian_update_velocity(delt, fric_zeta);    //update velocity the other half step
            gary[i].tforce = gary[i].sforce + gary[i].bforce + gary[i].ljforce + gary[i].eforce;
            gary[i].vel.x += (gary[i].vel.x*(-0.5*fric_zeta*delt)) + (gary[i].tforce.x*(0.5*delt/gary[i].m)) +
                             sqrt(2*6*delt*fric_zeta/gary[i].m)*(gsl_rng_uniform(r)-0.5);
            gary[i].vel.y += (gary[i].vel.y*(-0.5*fric_zeta*delt)) + (gary[i].tforce.y*(0.5*delt/gary[i].m)) +
                             sqrt(2*6*delt*fric_zeta/gary[i].m)*(gsl_rng_uniform(r)-0.5);
            gary[i].vel.z += (gary[i].vel.z*(-0.5*fric_zeta*delt)) + (gary[i].tforce.z*(0.5*delt/gary[i].m)) +
                             sqrt(2*6*delt*fric_zeta/gary[i].m)*(gsl_rng_uniform(r)-0.5);
        }




/*      __                 __                        ____     ________     ____
 *     /  \    |\    |    /  \    |       \   /     /    \        |       /    \
 *    |____|   | \   |   |____|   |        \ /      \_            |       \_
 *    |    |   |  \  |   |    |   |         |         \__         |         \__
 *    |    |   |   \ |   |    |   |         |            \        |            \
 *    |    |   |    \|   |    |   |_____    |       \____/    ____|____   \____/                                */





        if (a % 100 == 0) {                                           //analysis loop (energies)

            for (unsigned int i = 0; i < gary.size(); i++) {
                gary[i].ne = 0;                                     //blanking out energies here (ADD TO FXN 2-8)
                gary[i].be = 0;
                gary[i].ce = 0;
            }

            update_ES_energies(gary, lb, ni, qs);

            update_LJ_energies(gary, ecut, gpair);

            for (unsigned int i = 0; i < gedge.size(); i++) {
                if (gedge[i].type != 0) {
                    gedge[i].update_bending_energy(kb);
                }
            }

            for (unsigned int i = 0; i < gary.size(); i++) {
                gary[i].update_stretching_energy(ks, vdwr);
                gary[i].update_kinetic_energy();
                senergy += gary[i].se;                          //sum up total energies
                kenergy += gary[i].ke;
                ljenergy += gary[i].ne;
                benergy += gary[i].be;
                cenergy += gary[i].ce;
            }


            tenergy = senergy + kenergy + ljenergy + benergy + cenergy ; //print info to files for data analysis


            ofile << "ITEM: TIMESTEP" << endl << a << endl << "ITEM: NUMBER OF ATOMS" << endl << gary.size() << endl
                  << "ITEM: BOX BOUNDS" << endl << -bxsz.x / 2 << setw(15) << bxsz.x / 2 << endl << -bxsz.y / 2
                  << setw(15)
                  << bxsz.y / 2 << endl << -bxsz.z / 2 << setw(15) \
 << bxsz.z / 2 << endl << "ITEM: ATOMS index type x y z b charge" << endl;

            traj << a * delt << setw(15) << kenergy / gary.size() << setw(15) << senergy / gary.size() << setw(15) <<
                 benergy / gary.size() << setw(15) << ljenergy / gary.size() << setw(15) << cenergy / gary.size()
                 << setw(15) << tenergy / gary.size()
                 << setw(15) << (benergy + senergy + ljenergy + cenergy) / gary.size() << setw(15)
                 << kenergy * 2 / (3 * gary.size()) << setw(15) << endl;

            for (unsigned int b = 0; b < gary.size(); b++) {
                ofile << b + 1 << setw(15) << gary[b].type << setw(15) << gary[b].pos.x << setw(15) << gary[b].pos.y \
 << setw(15) << gary[b].pos.z << setw(15) << gary[b].be << setw(15) << gary[b].q << endl;

                count += 1;

            }
            senergy = 0;                            //blanking out energies
            kenergy = 0;
            benergy = 0;
            tenergy = 0;
            ljenergy = 0;
            cenergy = 0;

        }


        if (a % 1000 == 0) {
//            if (a > 100000) {
//                char filename[100];                                     //pair-correlation fxn file generation
//                filenumber += 1000;
//                sprintf(filename, "data.coords.all.%d", filenumber);
//                ofstream pairout(filename, ios::out);
//                for (int i = 0; i < gary.size(); i++) {
//                    if (gary[i].type == 3) {
//                        pairout << gary[i].id << setw(15) << gary[i].type << setw(15) << gary[i].m << setw(15)
//                                << gary[i].pos.x
//                                << setw(15) << gary[i].pos.y << setw(15) << gary[i].pos.z << endl;
//                    }
//                }
//            }

            int index = -1;

            for (unsigned int i = 0; i < garfield.size(); i++)           //Create oligomers for mass spectrum analysis
            {
                double oldsize = 0;

                if (garfield[i].itsO.size() == 0) {                       //if the unit isn't already counted...

                    ollie.push_back(OLIGOMER(VECTOR3D(0, 0, 0)));     //create an oligomer for the unit
                    index += 1;
                    ollie[index].itsU.push_back(&garfield[i]);      //add unit to oligomer
                    ollie[index].id = index;
                    garfield[i].itsO.push_back(ollie[index]);               //add oligomer to unit
                    while (oldsize < ollie[index].itsU.size()) {    //while the oligomer is still growing...
                        int n = oldsize;
                        oldsize = ollie[index].itsU.size();           //see how much the oligomer has grown
                        for (int j = n; j < oldsize; j++) {             //loop over the growth from last round
                            int g = ollie[index].itsU[j]->id;
                            for (int k = i + 1; k < garfield.size(); k++) { //look for new growth
                                if (garfield[k].itsO.size() == 0) {  //if it isn't in an oligomer yet...
                                    for (int m = 0;
                                         m < garfield[g].itsB.size(); m++) { //check to see if it is in this oligomer
                                        for (int n = 0; n < garfield[k].itsB.size(); n++) {
                                            if (dist(garfield[g].itsB[m], garfield[k].itsB[n]).GetMagnitude() < 1.5) {
                                                ollie[index].itsU.push_back(&garfield[k]);   //if it is attached, add it
                                                garfield[k].itsO.push_back(ollie[index]);   //mark subunit as bonded
                                                goto finish;
                                            }
                                        }
                                    }


                                }
                                finish:;
                            }
                        }

                    }
                }
            }
            mstime += 1;
            for (int i = 0; i < ollie.size(); i++) {
                if (ollie[i].itsU.size() >= 1) {
                    ms_bin[mstime][(ollie[i].itsU.size() - 1)] += 1;          //fill mass bins
                }
            }

            for (int j = 0; j < ms_bin[mstime].size(); j++) {
                msdata << ms_bin[mstime][j] << setw(15);                //print mass bin data to file
            }
            msdata << endl;

            for (int i = 0; i < garfield.size(); i++) {             // clear oligomer pointers from subunit
                garfield[i].itsO.clear();
            }

            ollie.erase(ollie.begin(),ollie.end());                 //erases oligomer objects
// g(r) dump data


        }//end of energy analysis loop


        double fraction_completed = ((a + 1) / (totaltime / delt));   //progress bar
        ProgressBar(fraction_completed);
    } //time loop end



    compute_MD_trust_factor_R(1);                   //computes R


}