#ifndef STRUCTS_H
#define STRUCTS_H

#include    <cstdlib>
#include    <cmath>
#include    <fstream>
#include    <sstream>
#include    <iostream>
#include    <iomanip>
#include    <string>
#include    <vector>
#include    <algorithm>
#include    <exception>
#include    <sys/time.h>
#include        <map>
#include        "feat.h"
#include <cstdint>
// fiber-positioner  -----------------------------------
class fpos {
    public:
    int fib_num; //number of fiber(!)  not positioner, read from fibpos file
    double fp_x; //x position in mm of positioner
    double fp_y; //y position in mm of positioner
    int location; //location of fiber
    int spectrom;//which spectrometer 0 - 9
    std::vector<int> N;// Identify neighboring positioners : neighbors of fiber k are N[k] 
    dpair coords; 
};

class FP  : public std::vector<struct fpos>{ 
    public:

    Table fibers_of_sp;// Inv map of spectrom, fibers_of_sp[sp] are fibers of spectrom sp, redundant
};

FP  read_fiber_positions(const Feat& F);
bool int_pairCompare(const std::pair<int, int>& firstElem, const std::pair<int, int>& secondElem);

// galaxy truth -------------------------------------------
class galaxy {
    public:
        long long targetid;      // the unique identifier
        long category;      // the true type when used with a secret file
        double z;
	uint16_t obsconditions;
};
class Gals : public std::vector<struct galaxy> {};

Gals read_Secretfile(str filename,const Feat& F);
Gals read_Secretfile_ascii(str filename,const Feat& F);

std::vector<int>count_galaxies(const Gals& G);

//target -----------------------------------------------------
class target {
    public:
  long long id; //note that this is how we read it in! not long
  int nobs_remain, nobs_done;
  double nhat[3];
  double ra, dec,subpriority;
  long desi_target, mws_target, bgs_target;
  int SS,SF,lastpass, priority_class, t_priority, once_obs;
  char brickname [9];
  Plist av_tfs;
  uint16_t obsconditions; // 16bit mask indicating under what conditions this target can be observed.
};
class MTL : public std::vector<struct target> {
    public:
    std::vector<int> priority_list;
};

MTL read_MTLfile(str filename, const Feat& F, int SS, int SF);

void assign_priority_class(MTL & M);


// Plate -------------------------------------------------
struct onplate { // The position of a galaxy in plate coordinates
    int id;
    double pos[2];
};

class Onplates : public std::vector<struct onplate> {};

class plate {
    public:
    int tileid;
    float tilera;
    float tiledec;
    double nhat[3]; // Unit vector pointing to plate
    int ipass; // Pass
    Table av_gals; // av_gals[k] : available galaxies of fiber k
    List density; // density[k] is the weighted number of objects available to (j,k)
    Table SS_av_gal;//SS_av_gal[p] are available standard stars on petal p of this plate
    Table SF_av_gal;
    Table SS_av_gal_fiber;
    Table SF_av_gal_fiber;
    std::vector<int> SS_in_petal;//number of SS assigned to a petal in this plate
    std::vector<int> SF_in_petal;
    bool is_used;  //true if tile has some galaxies within reach
    uint16_t obsconditions; // mask defining the kind of program (DARK, BRIGHT, GRAY)
    List av_gals_plate(const Feat& F, const MTL& M,const FP& pp) const; // Av gals of the plate
};
class Plates : public std::vector<struct plate> {};

Plates read_plate_centers(const Feat& F);


// Assignment ---------------------------------------------
// 2 mappings of assignments : (j,k) -> id(gal) ; id(gal)[5] -> (j,k)
class Assignment {
    public:
    //// ----- Members
    Table TF; // TF for tile fiber, #tiles X #fibers TF[j][k] is the chosen galaxy, -1 if not yet chosen
    List order; // Order of tiles we want to assign, only 1-n in simple increasing order for the moment
    List suborder; // Order of tiles actually containing targets. size is F.Nplate
    List inv_order; // inverse of suborder unused tiles map to -1
    int next_plate; // Next plate in the order, i.e suborder(next_plate) is actually next plate

    // Redundant information (optimizes computation time)
    Ptable GL; // GL for galaxy - list : #galaxies X (variable) #chosen TF: gives chosen tf's for galaxy g
    Cube kinds; // Cube[j][sp][id] : number of fibers of spectrometer sp and plate j that have the kind id
    Table unused; // Table [j][p] giving number of unused fibers on this petal
    //List nobsv; // List of nobs, redundant but optimizes, originally true goal
    //List nobsv_tmp; // List of nobs, redundant but optimizes, apparent goal, i.e. goal of category of this type, gets updated



    //// ----- Methods
    Assignment(const MTL& M, const Feat& F);
    ~Assignment();
    void assign(int j, int k, int g,  MTL& M, Plates& P, const FP& pp);
    void unassign(int j, int k, int g, MTL& M, Plates& P, const FP& pp);
    int find_collision(int j, int k, int g, const FP& pp, const MTL& M, const Plates& P, const Feat& F, int col=-1) const;
    bool find_collision(int j, int k, int kn, int g, int gn, const FP& pp, const MTL& M, const Plates& P, const Feat& F, int col=-1) const;
    int is_collision(int j, int k, const FP& pp, const MTL& M, const Plates& P, const Feat& F) const;
    void verif(const Plates& P, const MTL& M, const FP& pp, const Feat& F) const; // Verif mappings are right
    int is_assigned_jg(int j, int g) const;
    int is_assigned_jg(int j, int g, const MTL& M, const Feat& F) const;
    bool is_assigned_tf(int j, int k) const; 
    int na(const Feat& F, int begin=0, int size=-1) const; // Number of assignments (changes) within plates begin to begin+size
    int nobs(int g, const MTL& M, const Feat& F, bool tmp=true) const; // Counts how many more times object should be observed. If tmp=true, return maximum for this kind (temporary information)
    //if tmp=false we actually know the true type from the start
    Plist chosen_tfs(int g, const Feat& F, int begin=0) const; // Pairs (j,k) chosen by g, amongst size plates from begin
    int nkind(int j, int k, int kind, const MTL& M, const Plates& P, const FP& pp, const Feat& F, bool pet=false) const; // Number of fibers assigned to the kind "kind" on the petal of (j,k). If pet=true, we don't take k but the petal p directly instead
    List fibs_of_kind(int kind, int j, int pet, const MTL& M, const FP& pp, const Feat& F) const; // Sublist of fibers assigned to a galaxy of type kind for (j,p)
    //not used
    List sort_fibs_dens(int j, const List& fibs, const MTL& M, const Plates& P, const FP& pp, const Feat& F) const; // Sort this list of fibers by decreasing density
    List fibs_unassigned(int j, int pet, const MTL& M, const FP& pp, const Feat& F) const; // Sublist of unassigned fibers for (j,p)


    // Used to compute results at the end


    List unused_f(const Feat& F) const; //gives total number of unused fibers
    Table unused_fbp(const FP& pp, const Feat& F) const; // Unused fibers by petal
    float colrate(const FP& pp, const MTL& M, const Plates& P, const Feat& F, int j=-1) const; // Get collision rate, j = plate number
    int nobs_time(int g, int j, const Gals& Secret,const MTL& M, const Feat& F) const; // Know the number of remaining observations of g when the program is at the tile j, for pyplotTile

    int unused_f(int j, const Feat& F) const; // Number of unused fiber on the j'th plate
    //not used
    int unused_fbp(int j, int k, const FP& pp, const Feat& F) const; // Number of unassigned fibers of the petal corresponding to (j,k)
    //not used
    void update_nobsv_tmp(const Feat& F);
};

bool collision(dpair O1, dpair G1, dpair O2, dpair G2, const Feat& F); // collisions from  looking at galaxy G1 with fiber positioner centered at 01 and etc calculated in mm on plate

//int fprio(int g, const Gals& G, const Feat& F, const Assignment& A);//priority of galaxy g

double plate_dist(const double theta);//plate scale conversion
struct onplate change_coords(const struct target& O, const struct plate& P);
dpair projection(int g, int j, const MTL& M , const Plates& P); // Projection of g on j
int num_av_gals(int j, int k, const MTL& M, const Plates& P, const Feat& F, const Assignment& A); // weighted (and only with remaining observation according to the moment in the survey), and doesn't take into account other kinds than QSO LRG ELG not used

// Pyplot -----------------------------------------------
class pyplot {
    public:
    polygon pol;
    Slist text;
    Dplist textpos;

    pyplot(polygon p);
    void addtext(dpair p, str s);
    void plot_tile(str directory, int j, const Feat& F) const;
};


#endif
