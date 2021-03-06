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
#include    <stdexcept>
#include    <sys/time.h>
#include        <map>
#include        <stdlib.h>     /* srand, rand */
#include        "misc.h"
#include        "feat.h"
#include        "structs.h"
#include        "collision.h"
#include        "fitsio.h"
#include <string.h>
#include <cstdint>


// targets -----------------------------------------------------------------------

MTL read_MTLfile(str readfile, const Feat& F, int SS, int SF){
  //reads fits files, specifically mtl, standard stars, sky fibers
    str s = readfile;
    MTL M;
    std::string buf;
    const char* fname;
    fname = s.c_str();
    std::ifstream fs(fname);
    int ii;
    fitsfile *fptr;        
    int status = 0, anynulls;
    int hdutype;
    int nkeys;
    int hdupos;
    long nrows;
    long nkeep;
    int ncols;
    long long *targetid;
    long *desi_target;
    long *bgs_target;
    long *mws_target;
    int *numobs;
    int *priority;
    double *ra;
    double *dec;    
    int colnum;
    char **brickname;
    uint16_t *obsconditions;    
    double *subpriority;

    // General purpose output stream for exceptions
    std::ostringstream o;

    // Check that input file exists and is readable by cfitsio
    std::cout << "Finding file: " << fname << std::endl;
    int file_exists;
    fits_file_exists(fname,&file_exists,&status);
    std::ostringstream exists_str;
    exists_str << "(CFITSIO file_exists code: " << file_exists << ")";

    // Throw exceptions for failed read, see cfitsio docs
    if (! file_exists) {
        switch (file_exists){
        case -1:
            o << "Input MTL file must be a disk file: " << fname << " " << exists_str.str();
            throw std::runtime_error(o.str().c_str());
        case  0:
            o << "Could not find MTL input file: " << fname << " " << exists_str.str();
            throw std::runtime_error(o.str().c_str());
        case  2:
            o << "Cannot handle zipped MTL input file: " << fname << " " << exists_str.str();
            throw std::runtime_error(o.str().c_str());
        }
    }

    std::cout << "Found MTL input file: " << fname << std::endl;

    if (! fits_open_file(&fptr, fname, READONLY, &status) ){
      std::cout << "Reading MTL input file " << fname << std::endl;

      if ( fits_movabs_hdu(fptr, 2, &hdutype, &status) )
          myexit(status);
 
      fits_get_hdrspace(fptr, &nkeys, NULL, &status);            
      fits_get_hdu_num(fptr, &hdupos);
      fits_get_hdu_type(fptr, &hdutype, &status);  /* Get the HDU type */            
      fits_get_num_rows(fptr, &nrows, &status);
      fits_get_num_cols(fptr, &ncols, &status);

      fflush(stdout);
      if(!(targetid= (long long *)malloc(nrows * sizeof(long long)))){
        fprintf(stderr, "problem with targetid allocation\n");
        myexit(1);
      }
      if(!(desi_target= (long *)malloc(nrows * sizeof(long)))){
        fprintf(stderr, "problem with desi_target allocation\n");
        myexit(1);
      }
      if(!(mws_target= (long *)malloc(nrows * sizeof(long)))){
        fprintf(stderr, "problem with mws_target allocation\n");
        myexit(1);
      }
      if(!(bgs_target= (long *)malloc(nrows * sizeof(long)))){
        fprintf(stderr, "problem with bgs_target allocation\n");
        myexit(1);
      }
      if(!(numobs= (int *)malloc(nrows * sizeof(int)))){
        fprintf(stderr, "problem with numobs allocation\n");
        myexit(1);
      }
      if(!(priority= (int *)malloc(nrows * sizeof(int)))){
        fprintf(stderr, "problem with priority allocation\n");
        myexit(1);
      }
      if(!(subpriority= (double *)malloc(nrows * sizeof(double)))){
        fprintf(stderr, "problem with priority allocation\n");
        myexit(1);
      }
      if(!(ra= (double *)malloc(nrows * sizeof(double)))){
        fprintf(stderr, "problem with ra allocation\n");
        myexit(1);
      }      
      if(!(dec= (double *)malloc(nrows * sizeof(double)))){
        fprintf(stderr, "problem with dec allocation\n");
        myexit(1);
      }
      if(!(obsconditions = (uint16_t *)malloc(nrows * sizeof(int)))){
        fprintf(stderr, "problem with obsconditions allocation\n");
        myexit(1);
      }
      if(!(brickname= (char **)malloc(nrows * sizeof(char *)))){
        fprintf(stderr, "problem with brickname allocation\n");
        myexit(1);
      }
      for(ii=0;ii<nrows;ii++){
	if(!(brickname[ii]= (char *)malloc(9 * sizeof(char)))){
	  fprintf(stderr, "problem with brickname allocation\n");
	  myexit(1);
	}
      }
     
      //----- TARGETID
      /* find which column contains the TARGETID values */
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"TARGETID", &colnum, &status) ){
        fprintf(stderr, "error finding TARGETID column\n");
        myexit(status);
      }
      
      long frow, felem, nullval;
      frow = 1;
      felem = 1;
      nullval = -99.;
      if (fits_read_col(fptr, TLONGLONG, colnum, frow, felem, nrows, 
                        &nullval, targetid, &anynulls, &status) ){
        fprintf(stderr, "error reading TARGETID column\n");
        myexit(status);
      }




      
      //----- RA
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"RA", &colnum, &status) ){
        fprintf(stderr, "error finding RA column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TDOUBLE, colnum, frow, felem, nrows, 
                        &nullval, ra, &anynulls, &status) ){
        fprintf(stderr, "error reading RA column\n");
        myexit(status);
      }
      
      //----- DEC
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"DEC", &colnum, &status) ){
        fprintf(stderr, "error finding DEC column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TDOUBLE, colnum, frow, felem, nrows, 
        &nullval, dec, &anynulls, &status) ){
        fprintf(stderr, "error reading DEC column\n");
        myexit(status);
      }

      //----- Target mask bits
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"DESI_TARGET", &colnum, &status) ){
        fprintf(stderr, "error finding DESI_TARGET column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TLONG, colnum, frow, felem, nrows, 
                        &nullval, desi_target, &anynulls, &status) ){
        fprintf(stderr, "error reading DESI_TARGET column\n");
        myexit(status);
      }

      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"MWS_TARGET", &colnum, &status) ){
        fprintf(stderr, "error finding MWS_TARGET column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TLONG, colnum, frow, felem, nrows, 
                        &nullval, mws_target, &anynulls, &status) ){
        fprintf(stderr, "error reading MWS_TARGET column\n");
        myexit(status);
      }

      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"BGS_TARGET", &colnum, &status) ){
        fprintf(stderr, "error finding BGS_TARGET column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TLONG, colnum, frow, felem, nrows, 
                        &nullval, bgs_target, &anynulls, &status) ){
        fprintf(stderr, "error reading BGS_TARGET column\n");
        myexit(status);
      }
      
      // OBSCONDITIONS
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"OBSCONDITIONS", &colnum, &status) ){
        fprintf(stderr, "error finding OBSCONDITIONS column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, USHORT_IMG, colnum, frow, felem, nrows, 
                        &nullval, obsconditions, &anynulls, &status) ){
        fprintf(stderr, "error reading OBSCONDITIONS column\n");
        myexit(status);
      }

      //----- BRICKNAME
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"BRICKNAME", &colnum, &status) ){
	fprintf(stderr, "error finding BRICKNAME column\n");
	myexit(status);
      }
      if (fits_read_col(fptr, TSTRING, colnum, frow, felem, nrows, 
			&nullval, brickname, &anynulls, &status) ){
	fprintf(stderr, "error reading BRICKNAME column\n");
	myexit(status);
      }


      //----- SUBPRIORITY
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"SUBPRIORITY", &colnum, &status) ){
        fprintf(stderr, "error finding SUBPRIORITY column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TDOUBLE, colnum, frow, felem, nrows, 
        &nullval, subpriority, &anynulls, &status)){
        fprintf(stderr, "error reading SUBPRIORITY column\n");
        myexit(status);
      }



      //----- NUMOBS_MORE
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"NUMOBS_MORE", &colnum, &status) ){
        // fprintf(stderr, "error finding NUMOBS_MORE column\n");
        // myexit(status);
          std::cout << "NUMOBS_MORE not found ... setting to 0" << std::endl;
          for(int i=0; i<nrows; i++) {
              numobs[i] = 0;
          }
      } else if (fits_read_col(fptr, TINT, colnum, frow, felem, nrows, 
                        &nullval, numobs, &anynulls, &status) ){
        fprintf(stderr, "error reading NUMOBS_MORE column\n");
        myexit(status);
      }

      //----- PRIORITY
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"PRIORITY", &colnum, &status) ){
        // fprintf(stderr, "error finding PRIORITY column\n");
        // myexit(status);
        std::cout << "PRIORITY not found ... setting to 0" << std::endl;
        for(int i=0; i<nrows; i++) {
            priority[i] = 0;
        }
      } else if (fits_read_col(fptr, TINT, colnum, frow, felem, nrows, 
                        &nullval, priority, &anynulls, &status) ){
        fprintf(stderr, "error reading PRIORITY column\n");
        myexit(status);
      }

      

      
      // count how many rows we will keep and reserve that amount
      nkeep = 0;
      for(int i=0; i<nrows; i++){
        if(F.MinRa <= ra[i] && ra[i] <= F.MaxRa &&
           F.MinDec <= dec[i] && dec[i] <= F.MaxDec ) {
             nkeep++;
        }
      }
      printf("Keeping %d targets within ra/dec ranges\n", nkeep);
      try {M.reserve(nkeep);} catch (std::exception& e) {myexception(e);}

      for(ii=0;ii<nrows;ii++){
        str xname;

	// make sure ra is between 0 and 360
        if (ra[ii]<   0.) {ra[ii] += 360.;}
        if (ra[ii]>=360.) {ra[ii] -= 360.;}
        if (dec[ii]<=-90. || dec[ii]>=90.) {
          std::cout << "DEC="<<dec[ii]<<" out of range reading "<<fname<<std::endl;
          myexit(1);
        }
        
        if(F.MinRa <= ra[ii] && ra[ii] <= F.MaxRa &&
           F.MinDec <= dec[ii] && dec[ii] <= F.MaxDec ) {
             double theta = (90.0 - dec[ii])*M_PI/180.;
             double phi   = (ra[ii]        )*M_PI/180.;
             struct target Q;
             Q.nhat[0]    = cos(phi)*sin(theta);
             Q.nhat[1]    = sin(phi)*sin(theta);
             Q.nhat[2]    = cos(theta);
	     Q.obsconditions = obsconditions[ii];
             Q.t_priority = priority[ii];//priority not present for sky fibers or standard stars
             Q.subpriority = subpriority[ii];
             Q.nobs_remain= numobs[ii];
             Q.nobs_done=0;//need to keep track of this, too
             Q.once_obs=0;//changed only in update_plan
             Q.ra = ra[ii];
             Q.dec = dec[ii];
             Q.id = targetid[ii];
             Q.desi_target = desi_target[ii];
             Q.mws_target = mws_target[ii];
             Q.bgs_target = bgs_target[ii];
             Q.SS=SS;
             Q.SF=SF;
	     strncpy(Q.brickname, brickname[ii], 9);
             try{M.push_back(Q);}catch(std::exception& e) {myexception(e);}
 
             bool in=false;
             for (int j=0;j<M.priority_list.size();++j){
               if(Q.t_priority==M.priority_list[j]){
                 in=true;
               }
             }
             if(!in){
               M.priority_list.push_back(Q.t_priority);
             }
           }  // end if within RA,dec bounds
      } // end ii loop over targets
      std::sort(M.priority_list.begin(),M.priority_list.end());

      return(M);  
    } else {
        std::ostringstream open_status_str;
        open_status_str << "(CFITSIO open_file status: " << status << ")";
        o << "Problem opening input MTL fits file: " << fname << " " << open_status_str.str();
        throw std::runtime_error(o.str().c_str());
    }
}


void assign_priority_class(MTL& M){
    // assign each target to a priority class
    //this needs to be updated
    for(int i=0;i<M.size();++i){
        if(!M[i].SS&&!M[i].SF){
            for(int j=0;j<M.priority_list.size();++j){
                if(M[i].t_priority==M.priority_list[j]){
                    M[i].priority_class=j;}
            }
        }
    }
}


// FP ----------------------------------------------------------------------------
// Read the positions of the fibers on each plate.
// need also to get the petal, i.e. spectrometer  rnc 1/16/15  added S
FP  read_fiber_positions(const Feat& F) {
    std::string buf;
    std::ifstream fs(F.fibFile.c_str());

    if (!fs) { // An error occurred opening the file.
        std::cerr << "Unable to open file " << F.fibFile << std::endl;
        myexit(1);
    }

    getline(fs,buf);
    while (fs.eof()==0 && ( (buf[0]=='#') || (buf.size()==0) )) {
        getline(fs,buf);
    }
    int i(0);
    int petals_pac[] = {0,1,2,7,8,9};
    List petals_pacL = initList(petals_pac,6);
    List inv = inverse(petals_pacL);
    FP FibPos; //collection of fibersfound(spectro,petals_pacL)) {
    fpos fiber_pos;
    printf("before reading positioners \n");
    std::cout.flush();
    while (fs.eof()==0) {
        double x,y; int fiber,location,spectro,remove; 
        std::istringstream(buf) >> fiber >> location >> spectro >> x >> y;
        try{
            fiber_pos.fib_num=fiber;
            fiber_pos.location=location;
            fiber_pos.fp_x=x;
            fiber_pos.fp_y=y;
            int sp = F.Pacman ? inv[spectro] : spectro;
            fiber_pos.spectrom=spectro;  
            fiber_pos.coords=dpair(x,y);
        } catch(std::exception& e) {myexception(e);}
    
        FibPos.push_back(fiber_pos);
        getline(fs,buf);
        i++;
    }

    fs.close();
    printf("read the positioner file\n");

    int fiber_size=FibPos.size();

    //sort by fiber number

    std::vector<std::pair<int,int> >pairs;
    for(int f=0;f<FibPos.size();++f){
       std::pair <int,int> this_pair (FibPos[f].fib_num,f);
       pairs.push_back(this_pair);
    }
    std::sort(pairs.begin(),pairs.end(),int_pairCompare);
    std::vector<fpos>out;
    for(int f=0;f<FibPos.size();++f){
       out.push_back(FibPos[pairs[f].second]);
    }
    copy(out.begin(),out.end(),FibPos.begin());
    printf(" sorted by fiber number\n");
    for (int i=0;i<10;++i){
      printf(" i %d FibPos[i].fib_num %d \n",i, FibPos[i].fib_num);
    }

    //create fibers_of_sp
    FibPos.fibers_of_sp.resize(F.Npetal);
    for (int k=0; k<fiber_size; k++){
      FibPos.fibers_of_sp[FibPos[k].spectrom].push_back(k);
    }

   //create table of Neighbors    
    for(int i=0; i<fiber_size; i++) {
        for (int j=0; j<fiber_size; j++) {
            if(i!=j) {
                if(sq(FibPos[i].fp_x-FibPos[j].fp_x)+sq(FibPos[i].fp_y-FibPos[j].fp_y) < sq(F.NeighborRad)) {
                    FibPos[i].N.push_back(j); }
            }
        }
    }   
    printf(" made neighbors \n");
    return (FibPos);
}

//FP::FP() {};


// plate ---------------------------------------------------------------------------

List plate::av_gals_plate(const Feat& F,const MTL& M, const FP& pp) const {//list of galaxies available to plate no repetitions
    List gals = initList(F.Ngal);
    List L = initList(0);

    for (int k=0; k<F.Nfiber; k++) {
        for (int i=0; i<av_gals[k].size(); i++) {
            if (gals[av_gals[k][i]] == 0) {
                gals[av_gals[k][i]] = 1;
                L.push_back(i);
            }
        }
    }
    return L;
}



Plates read_plate_centers(const Feat& F) {
    Plates P,PQ;
    const char* fname;

    /*Variables used to read fits file*/
    fitsfile *fptr;        
    int status = 0, anynulls;
    int hdutype;
    int nkeys;
    int hdupos;
    long nrows;
    long nkeep;
    int ncols;
    int ii;
    uint16_t *obsconditions;    
    int *in_desi;
    int *tile_id;   
    int tileid;
    int *ipass;
    double *ra;
    double *dec;   
    int colnum;
    long frow, felem, nullval;
    frow = 1;
    felem = 1;
    nullval = -99.;
    Time t, time; // t for global, time for local
    init_time(t);
    // read the strategy file
    // survey_list is list of tiles specified by tileid (arbitrary int) in order of survey
    std::ifstream fsurvey(F.surveyFile.c_str());
    if (!fsurvey) {  // An error occurred opening the file.
        std::cerr << "Unable to open file " << F.surveyFile << std::endl;
        myexit(1);
    }
    int survey_tile;

    std::vector<int> survey_list;
    std::string buf;
    while(getline(fsurvey,buf)){
        std::istringstream ss(buf);
        if(!(ss>>survey_tile)){break;}
        survey_list.push_back(survey_tile);
    }
    printf(" number of tiles %d \n",survey_list.size());

    // NEW
    // read list of tile centers
    // Check that input file exists and is readable by cfitsio
    fname = F.tileFile.c_str();
    std::cout << "Finding file: " << fname << std::endl;
    int file_exists;
    fits_file_exists(fname,&file_exists,&status);
    std::ostringstream exists_str;
    exists_str << "(CFITSIO file_exists code: " << file_exists << ")";
    std::ostringstream o;

    // Throw exceptions for failed read, see cfitsio docs
    if (! file_exists) {
        switch (file_exists){
        case -1:
            o << "Input tile centers file must be a disk file: " << fname << " " << exists_str.str();
            throw std::runtime_error(o.str().c_str());
        case  0:
            o << "Could not find input tile centers file: " << fname << " " << exists_str.str();
            throw std::runtime_error(o.str().c_str());
        case  2:
            o << "Cannot handle zipped tile centers input file: " << fname << " " << exists_str.str();
            throw std::runtime_error(o.str().c_str());
        }
    }

    std::cout << "Found input tile centers file: " << fname << std::endl;

    if (! fits_open_file(&fptr, fname, READONLY, &status) ){
      std::cout << "Reading input tile centers file " << fname << std::endl;

      if ( fits_movabs_hdu(fptr, 2, &hdutype, &status) )
	myexit(status);
      
      fits_get_hdrspace(fptr, &nkeys, NULL, &status);            
      fits_get_hdu_num(fptr, &hdupos);
      fits_get_hdu_type(fptr, &hdutype, &status);  /* Get the HDU type */            
      fits_get_num_rows(fptr, &nrows, &status);
      fits_get_num_cols(fptr, &ncols, &status);
      /*
      std::cout << ncols << " columns " << nrows << "nrows" << std::endl;
      std::cout << "HDU " << hdupos << std::endl;
      if (hdutype == ASCII_TBL){
	std::cout << "ASCII TABLE: " << std::endl;
      }else{
	std::cout << "BINARY TABLE: " << std::endl;
      }
      */

      
      if(!(obsconditions = (uint16_t *)malloc(nrows * sizeof(int)))){
        fprintf(stderr, "problem with priority allocation\n");
        myexit(1);
      }


      if(!(ipass = (int *)malloc(nrows * sizeof(int)))){
        fprintf(stderr, "problem with ipass allocation\n");
        myexit(1);
      }

      if(!(in_desi = (int *)malloc(nrows * sizeof(int)))){
        fprintf(stderr, "problem with priority allocation\n");
        myexit(1);
      }

      if(!(tile_id = (int *)malloc(nrows * sizeof(int)))){
        fprintf(stderr, "problem with priority allocation\n");
        myexit(1);
      }

      if(!(ra= (double *)malloc(nrows * sizeof(double)))){
        fprintf(stderr, "problem with ra allocation\n");
        myexit(1);
      }      
      if(!(dec= (double *)malloc(nrows * sizeof(double)))){
        fprintf(stderr, "problem with dec allocation\n");
        myexit(1);
      }

      //----- RA
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"RA", &colnum, &status) ){
        fprintf(stderr, "error finding RA column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TDOUBLE, colnum, frow, felem, nrows, 
                        &nullval, ra, &anynulls, &status) ){
        fprintf(stderr, "error reading RA column\n");
        myexit(status);
      }
      
      //----- DEC
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"DEC", &colnum, &status) ){
        fprintf(stderr, "error finding DEC column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TDOUBLE, colnum, frow, felem, nrows, 
			&nullval, dec, &anynulls, &status) ){
        fprintf(stderr, "error reading DEC column\n");
        myexit(status);
      }
      
      //----- IN_DESI
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"IN_DESI", &colnum, &status) ){
        fprintf(stderr, "error finding IN_DESI column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TINT, colnum, frow, felem, nrows, 
                        &nullval, in_desi, &anynulls, &status) ){
        fprintf(stderr, "error reading IN_DESI column\n");
        myexit(status);
      }


      //----- OBSCONDITIONS
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"OBSCONDITIONS", &colnum, &status) ){
        fprintf(stderr, "error finding OBSCONDITIONS column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, USHORT_IMG, colnum, frow, felem, nrows, 
                        &nullval, obsconditions, &anynulls, &status) ){
        fprintf(stderr, "error reading OBSCONDITIONS column\n");
        myexit(status);
      }

      //----- TILEID
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"TILEID", &colnum, &status) ){
        fprintf(stderr, "error finding OBSCONDITIONS column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TINT, colnum, frow, felem, nrows, 
                        &nullval, tile_id, &anynulls, &status) ){
        fprintf(stderr, "error reading TILEID column\n");
        myexit(status);
      }


      //----- PASS
      if ( fits_get_colnum(fptr, CASEINSEN, (char *)"PASS", &colnum, &status) ){
        fprintf(stderr, "error finding PASS column\n");
        myexit(status);
      }
      if (fits_read_col(fptr, TINT, colnum, frow, felem, nrows, 
                        &nullval, ipass, &anynulls, &status) ){
        fprintf(stderr, "error reading PASS column\n");
        myexit(status);
      }

      try {P.reserve(400000);} catch (std::exception& e) {myexception(e);}      
      for(ii=0;ii<nrows;ii++){
	//	fprintf(stdout, "in desi %d\n", in_desi[ii]);
        if ((in_desi[ii]==1) && (obsconditions[ii]!=0)) {	  
            if (ra[ii]<   0.) {ra[ii] += 360.;}
            if (ra[ii]>=360.) {ra[ii] -= 360.;}
            if (dec[ii]<-90. || dec[ii]>90.) {
                std::cout << "DEC="<<dec<<" out of range reading " << F.tileFile<<std::endl;
                myexit(1);
            }
            double theta = (90.0 - dec[ii])*M_PI/180.;
            double phi   = (ra[ii]        )*M_PI/180.;
            struct plate Q;

            Q.tileid = tile_id[ii];
	    Q.obsconditions = obsconditions[ii];
            //                        std::cout << "TILEID " << tileid << std::endl;
            Q.tilera        = ra[ii];
            Q.tiledec       = dec[ii];
            Q.nhat[0]    = sin(theta)*cos(phi);
            Q.nhat[1]    = sin(theta)*sin(phi);
            Q.nhat[2]    = cos(theta);
            Q.ipass      = ipass[ii]; // <- be careful, format of input file
            Q.av_gals.resize(F.Nfiber); // <- added
            Q.density.resize(F.Nfiber); // <- added
            Q.SS_av_gal.resize(F.Npetal);//was Nfbp
            Q.SF_av_gal.resize(F.Npetal);//was Nfbp
            Q.SS_in_petal.resize(F.Npetal);
            Q.SF_in_petal.resize(F.Npetal);
            Q.SS_av_gal_fiber.resize(F.Nfiber);
            Q.SF_av_gal_fiber.resize(F.Nfiber);
            for (int i=0;i<F.Npetal;++i){Q.SS_in_petal[i]=0;}
            for (int i=0;i<F.Npetal;++i){Q.SF_in_petal[i]=0;}
            //if(dec<F.MaxDec && dec>F.MinDec &&ra<F.MaxRa && ra>F.MinRa){
	    try {P.push_back(Q);} catch(std::exception& e) {myexception(e);}
	}
      }
    }

    printf(" size of P  %d\n",P.size());


    // Map each valid tileid in order to an index in P[].
    // Tileid is an arbitrary int
    std::map<int,int> invert_tile;
    std::map<int,int>::iterator tileid_to_idx;
    std::pair<std::map<int,int>::iterator,bool> ret;
    init_time_at(time,"# Start invert tiles",t);
    for(unsigned i=0;i<P.size();++i)
    {
        ret = invert_tile.insert(std::make_pair(P[i].tileid,i));
        // Check for duplicates (std::map.insert only creates keys, fails on duplicate keys)
        if ( ret.second == false ) {
            std::ostringstream o;
            o << "Duplicate tileid " << P[i].tileid << " in tileFile!";
            throw std::logic_error(o.str().c_str());
        }
    }
    print_time(time,"# ..inversion  took :");
    // Create PQ, a subset of P containing those tileids specified in the
    // surveyFile, in the order in which they are specified.
    init_time_at(time,"# do inversion of used plates",t);
    for(unsigned i=0;i<survey_list.size();++i){
        tileid        = survey_list[i];
        tileid_to_idx = invert_tile.find(tileid);

        if (tileid_to_idx == invert_tile.end()){
            // Can end up with no mapping if surveyFile contains valid tileids that have in_desi = 0 in the tileFile.
            std::ostringstream o;
            o << "surveyFile contains tileid " << tileid << ", which is not included (or has in_desi = 0) in tileFile.";
            throw std::range_error(o.str().c_str());
        }

        // Found a valid index, push the tile to the ordered list.
        PQ.push_back(P[tileid_to_idx->second]);
    }
    print_time(time,"# .. sued plates inversion  took :");
    return(PQ);
}
// Assignment -----------------------------------------------------------------------------
Assignment::Assignment(const MTL& M, const Feat& F) {

    TF=initTable(F.Nplate,F.Nfiber,-1);//galaxy assigned to tile-fiber TF[j][k]
    GL = initPtable(F.Ngal,0); //tile-fiber pair for galaxy  GL[g]
    inv_order=initList(F.Nplate,-1);
    next_plate = 0;
    kinds = initCube(F.Nplate,F.Npetal,F.Categories);
    unused = initTable(F.Nplate,F.Npetal,F.Nfbp);//initialized to number of fibers on a petal
    }

Assignment::~Assignment() {}

// Assign g with tile/fiber (j,k), and check for duplicates
void Assignment::assign(int j, int k, int g, MTL& M, Plates& P, const FP& pp) {

    // Assign (j,k)
    int q = TF[j][k];
    if (q != -1) {
        printf("### !!! ### DUPLICATE (j,k) = (%d,%d) assigned with g = %d and %d ---> information on first g lost \n",j,k,q,g);
        myexit(1);
    }

    // Assign g
    TF[j][k]=g;
    Plist pl = GL[g];//pair list, tf's for this g
    pair p = pair(j,k);
    for(int i=0;i<pl.size();i++){
        if(pl[i].f==j){

        printf("### !!! ### DUPLICATE g = %d assigned with (j,k) = (%d,%d) and (%d,%d) ---> information on first (j,k) lost \n",g,pl[i].f,pl[i].s,j,k);
        
        myexit(1); // Can be commented if want to force continuing
        }
    }
    GL[g].push_back(p);
    M[g].nobs_done++;
    M[g].nobs_remain--;
    if(M[g].SF){
        int q=pp[k].spectrom;
        P[j].SF_in_petal[q]+=1;}
    if(M[g].SS){
        int q=pp[k].spectrom;
        P[j].SS_in_petal[q]+=1;}
    unused[j][pp[k].spectrom]--;

}

void Assignment::unassign(int j, int k, int g, MTL& M, Plates& P, const FP& pp) {
  //diagnostic

    if (TF[j][k]==-1) printf("### !!! ### TF (j,k) = (%d,%d) gets unassigned but was already not assigned\n",j,k);
    int a = isfound(pair(j,k),GL[g]);
    if (a==-1) printf("### !!! ### Galaxy g = %d gets unassigned but was already not assigned\n",g);

    TF[j][k] = -1;
    if (a!=-1) erase(a,GL[g]);
    M[g].nobs_done--;
    M[g].nobs_remain++;
    if(M[g].SF){
        int p=pp[k].spectrom;
        P[j].SF_in_petal[p]-=1;}
    if(M[g].SS){
        int p=pp[k].spectrom;
        P[j].SS_in_petal[p]-=1;}

    unused[j][pp[k].spectrom]++;

}

void Assignment::verif(const Plates& P, const MTL& M, const FP& pp, const Feat& F) const {
    str qso_lrgA[] = {"QSOLy-a","QSOTracer","FakeQSO","LRG","FakeLRG"}; List qso_lrg = F.init_ids_list(qso_lrgA,5);
    for (int g=0; g<F.Ngal; g++) {// make sure observations are separated by at least InterPlate
        Plist tfs = GL[g];
        int j0(-1); int j1(-1);
        for (int i=0; i<tfs.size(); i++) {
            pair tf = tfs[i];
            int j0 = j1;
            int j1 = tf.f;

            if (TF[tf.f][tf.s]!=g) { printf("ERROR in verification of correspondance of galaxies !\n"); fl(); }
            // No 2 assignments within an interval of F.InterPlate
            //if (j0!=-1 && isfound(M[g].id,qso_lrg) && fabs(j1-j0)<F.InterPlate) { printf("ERROR in verification of F.InterPlate g=%d with j=%d and %d\n",g,j0,j1); fl(); }
        }
    }
    for (int j=0; j<F.Nplate; j++) {
        List gals = initList(F.Ngal);
        for (int k=0; k<F.Nfiber; k++) {
            int g = TF[j][k];
            if (g!=-1) {
                // Verif on GL: is it consistent with TF?
                if (isfound(pair(j,k),GL[g])==-1) { printf("ERROR in verification of correspondance of tfs !\n"); fl(); }
                // Verif that a galaxy isn't observed twice
                if (gals[g]==1) printf("ERROR in verification, twice the same galaxy by (%d,%d)\n",j,k);
                else gals[g] = 1;
                // Collision checking
                if (!F.Collision && is_collision(j,k,pp,M,P,F)!=-1) printf("ERROR in verification : collisions\n");
            }
        }
    }
    for (int j=0; j<F.Nplate; j++) {// right number of SS and SF
        for (int n=0; n<F.Npetal; n++){
            if (kinds[j][n][F.ids.at("SS")]!=F.MaxSS || kinds[j][n][F.ids.at("SF")]!=F.MaxSF) printf("ERROR in verification : number of SF or SS\n");
        }
    }
}

int Assignment::is_assigned_jg(int j, int g) const {// is galaxy g assigned on tile j
    for (int i=0; i<GL[g].size(); i++) if (GL[g][i].f == j) return i;
    return -1;
}

int Assignment::is_assigned_jg(int j, int g, const MTL& M, const Feat& F) const { // No occurrence too nearby in tiles
    for (int i=0; i<GL[g].size(); i++) if (fabs(j-GL[g][i].f)<F.InterPlate || j==GL[g][i].f) return i; 
    return -1;
}

bool Assignment::is_assigned_tf(int j, int k) const { return (TF[j][k] != -1); }

int Assignment::na(const Feat& F, int begin, int size) const {//unassigned fibers in tiles begin to begin+size
    int size1 = (size==-1) ? F.Nplate : size;
    int cnt(0);
    for (int j=begin; j<begin+size1; j++) {
        for (int k=0; k<F.Nfiber; k++) {
            if (TF[j][k]!=-1) cnt++;
        }
    }
    return cnt;
}

Plist Assignment::chosen_tfs(int g, const Feat& F, int begin) const {//creates list of tile-fibers observing g starting from plate begin
    Plist chosen;
    for (int i=0; i<GL[g].size(); i++) {
        pair tf = GL[g][i];
        if (begin<=tf.f ) {
            if (TF[tf.f][tf.s]!=g) { printf("ERROR in chosen_tfs\n"); fl(); }
            chosen.push_back(tf);
        }
    }
    return chosen;
}

Table Assignment::unused_fbp(const FP& pp, const Feat& F) const {//table unused fibers on petal  on tile  j
    Table unused = initTable(F.Nplate,F.Npetal);
    //List Sp = pp.spectrom;
    for(int j=0; j<F.Nplate; j++) {
        for (int k=0; k<F.Nfiber; k++) {
            if (!is_assigned_tf(j,k)) unused[j][pp[k].spectrom]++;
        }
    }
    return unused;
}
//not used
List Assignment::unused_f(const Feat& F) const {//total unused fibers
    List unused = initList(F.Nplate);
    for(int j=0; j<F.Nplate; j++) {
        for (int k=0; k<F.Nfiber; k++) {
            if (!is_assigned_tf(j,k)) unused[j]++;
        }
    }
    return unused;
}

int Assignment::unused_f(int j, const Feat& F) const {//unused fibers on tile j
    int unused(0);
    for (int k=0; k<F.Nfiber; k++) if (!is_assigned_tf(j,k)) unused++;
    return unused;
}
int Assignment::unused_fbp(int j, int k, const FP& pp, const Feat& F) const {//unused fibers on petal containing fiber k, tile j
    List fibs = pp.fibers_of_sp[pp[k].spectrom];
    int unused(0);
    for (int i=0; i<fibs.size(); i++) {
        if (!is_assigned_tf(j,fibs[i])) unused++;
    }
    return unused;
}

int Assignment::nkind(int j, int k, int kind, const MTL& M, const Plates& P, const FP& pp, const Feat& F, bool pet) const {
    //if pet is false, used petal of fiber k,, if pet is true use petal k        
    if (!pet) return kinds[j][pp[k].spectrom][kind];
    else return kinds[j][k][kind];
}
List Assignment::fibs_unassigned(int j, int pet, const MTL& M, const FP& pp, const Feat& F) const {//list of unassigned fibers on petal pet
    List L;
    List fibs = pp.fibers_of_sp[pet];
    for (int kk=0; kk<F.Nfbp; kk++) {
        int k = fibs[kk];
        if (!is_assigned_tf(j,k)) L.push_back(k);
    }
    return L;
}


int Assignment::nobs_time(int g, int j, const Gals& Secret, const MTL& M,const Feat& F) const {
    //gives required number of observations after jth tile  rnc 6/1/16
    //used in pyplotTile
    int kind = Secret[g].category;
    int cnt = M[g].once_obs ? F.goalpost[kind] : F.goal[kind];
    for (int i=0; i<GL[g].size(); i++) if (GL[g][i].f<j) cnt--;
    return cnt;
}
// Returns the radial distance on the plate (mm) given the angle,
// theta (radians).  This is simply a fit to the data provided.
double plate_dist(const double theta) {
    const double p[4] = {8.297e5,-1750.,1.394e4,0.0};
    double rr=0;
    for (int i=0; i<4; i++) rr = theta*rr + p[i];
    return rr;
}

// Returns the x-y position on the plate centered at P for galaxy O.
struct onplate change_coords(const struct target& O, const struct plate& P) {
    struct onplate obj;
    // Rotate the "galaxy" vector so that the plate center is at z-hat.
    double nhat1[3],nhat2[3];
    const double ct=P.nhat[2],st=sqrt(1-P.nhat[2]*P.nhat[2])+1e-30;
    const double cp=P.nhat[0]/st,sp=P.nhat[1]/st;
    // First rotate by -(Phi-Pi/2) about z. Note sin(Phi-Pi/2)=-cos(Phi)
    // and cos(Phi-Pi/2)=sin(Phi).
    nhat1[0] =  O.nhat[0]*sp - O.nhat[1]*cp;
    nhat1[1] =  O.nhat[0]*cp + O.nhat[1]*sp;
    nhat1[2] =  O.nhat[2];
    // then rotate by Theta about x
    nhat2[0] =  nhat1[0];
    nhat2[1] =  nhat1[1]*ct - nhat1[2]*st;
    nhat2[2] =  nhat1[1]*st + nhat1[2]*ct;
    // now work out the "radius" on the plate
    double tht=sqrt(sq(nhat2[0],nhat2[1]));
    double rad=plate_dist(tht);
    // the x-y position is given by our nhat's scaled by this
    obj.pos[0] = nhat2[0]/tht * rad;
    obj.pos[1] = nhat2[1]/tht * rad;
    return obj; 
}


bool collision(dpair O1, dpair G1, dpair O2, dpair G2, const Feat& F) {
    double dist_sq = sq(G1,G2);
    if (dist_sq < sq(F.Collide)) return true;
    if (dist_sq > sq(F.NoCollide)) return false;
    PosP posp(3,3);
    polygon fh1 = F.fh;
    polygon fh2 = F.fh;
    polygon cb1 = F.cb;
    polygon cb2 = F.cb;
    repos_cb_fh(cb1,fh1,O1,G1,posp);
    repos_cb_fh(cb2,fh2,O2,G2,posp);
    if (collision(fh1,fh2)) return true;
    if (collision(cb1,fh2)) return true;
    if (collision(cb2,fh1)) return true;
    return false;
}

// (On plate p) finds if there is a collision if fiber k would observe galaxy g (collision with neighbor)
//  j is in list that runs to F.Nplate since it is used in TF[j][k]
int Assignment::find_collision(int j, int k, int g, const FP& pp, const MTL& M, const Plates& P, const Feat& F, int col) const {//check all neighboring fibers
    bool bol = (col==-1) ? F.Collision : false;
    if (bol) return -1;
    dpair G1 = projection(g,j,M,P);
    for (int i=0; i<pp[k].N.size(); i++) {// i numbers the fibers neighboring fiber k
        int kn = pp[k].N[i];
        int gn = TF[j][kn];
        if (gn!=-1) {
            dpair G2 = projection(gn,j,M,P);
            bool b = F.Exact ? collision(pp[k].coords,G1,pp[kn].coords,G2,F) : (sq(G1,G2) < sq(F.AvCollide));
            if (b) return kn;
        }
    }
    return -1;
}

bool Assignment::find_collision(int j, int k, int kn, int g, int gn, const FP& pp, const MTL& M, const Plates& P, const Feat& F, int col) const {//check two fibers
    bool bol = (col==-1) ? F.Collision : false;
    if (bol) return false;
    dpair G1 = projection(g,j,M,P);
    dpair G2 = projection(gn,j,M,P);
    return F.Exact ? collision(pp[k].coords,G1,pp[k].coords,G2,F) : (sq(G1,G2) < sq(F.AvCollide));
}

int Assignment::is_collision(int j, int k, const FP& pp, const MTL& M, const Plates& P, const Feat& F) const {
    //find collision for galaxy g
    int g = TF[j][k];
    if (g!=-1) return find_collision(j,k,g,pp,M,P,F,0);
    else return -1;
}

float Assignment::colrate(const FP& pp, const MTL& M, const Plates& P, const Feat& F, int jend0) const {
    //rate of collisions
    int jend = (jend0==-1) ? F.Nplate : jend0;
    int col = 0;
    for (int j=0; j<jend; j++) {
        List done = initList(F.Nfiber);
        for (int k=0; k<F.Nfiber; k++) {
            if (done[k] == 0) {
                int c = is_collision(j,k,pp,M,P,F);
                if (c!=-1) {
                    done[c] = 1;
                    col += 2;
                }
            }
        }
    }
    return percent(col,jend*F.Nfiber);
}

dpair projection(int g, int j, const MTL& M, const Plates& OP) {
    //x and y coordinates for galaxy observed on plate j
    struct onplate op = change_coords(M[g],OP[j]);
    return dpair(op.pos[0],op.pos[1]);
}


pyplot::pyplot(polygon p) {
    pol = p;
}

void pyplot::addtext(dpair p, str s) {
    text.push_back(s);
    textpos.push_back(p);
}

void pyplot::plot_tile(str directory, int j, const Feat& F) const {
    FILE * file;
    str fname = directory+"/tile"+i2s(j)+".py";
    file = fopen(fname.c_str(),"w");

    // Header
    Dlist lims = pol.limits();
    fprintf(file,"from pylab import *\nimport pylab as pl\nimport matplotlib.pyplot as plt\nfrom matplotlib import collections as mc\nax=subplot(aspect='equal')\naxes = plt.gca()\naxes.set_xlim([%f,%f])\naxes.set_ylim([%f,%f])\nax.axis('off')\nax.get_xaxis().set_visible(False)\nax.get_yaxis().set_visible(False)\nset_cmap('hot')\nfig = plt.gcf()\n\n",lims[0],lims[1],lims[2],lims[3]);
    if (j!=-1) fprintf(file,"plt.text(350,-350,'Tile %d',horizontalalignment='center',verticalalignment='center',size=5)\n\n",j);
    if (j!=-1) fprintf(file,"plt.text(200,-375,'QSO-Ly-a',color='black',horizontalalignment='center',verticalalignment='center',size=4)\n\n",j);
    if (j!=-1) fprintf(file,"plt.text(250,-375,'QSO-tracer',color='green',horizontalalignment='center',verticalalignment='center',size=4)\n\n",j);
    if (j!=-1) fprintf(file,"plt.text(300,-375,'LRG',color='red',horizontalalignment='center',verticalalignment='center',size=4)\n\n",j);
    if (j!=-1) fprintf(file,"plt.text(350,-375,'ELG',color='blue',horizontalalignment='center',verticalalignment='center',size=4)\n\n",j);
    if (j!=-1) fprintf(file,"plt.text(200,-400,'Fake QSO',color='m',horizontalalignment='center',verticalalignment='center',size=4)\n\n",j);
    if (j!=-1) fprintf(file,"plt.text(250,-400,'Fake LRG',color='y',horizontalalignment='center',verticalalignment='center',size=4)\n\n",j);
    if (j!=-1) fprintf(file,"plt.text(300,-400,'Std. Star',color='gray',horizontalalignment='center',verticalalignment='center',size=4)\n\n",j);
    if (j!=-1) fprintf(file,"plt.text(350,-400,'Sky Fiber',color='c',horizontalalignment='center',verticalalignment='center',size=4)\n\n",j);
    
    // Plot polygon
    for (int i=0; i<pol.elmts.size(); i++) {
        element e = pol.elmts[i];
        if (e.is_seg) {
            if (1<e.segs.size()) {
                fprintf(file,"lines = [[");
                for (int j=0; j<e.segs.size(); j++) fprintf(file,"(%f,%f),",e.segs[j].f,e.segs[j].s);
                if (e.color!='w') fprintf(file,"]]\nlc = mc.LineCollection(lines,linewidths=0.2,color='%c')\nax.add_collection(lc)\n",e.color);
                else fprintf(file,"]]\nlc = mc.LineCollection(lines,linewidths=0.2,color='k',alpha=0.4)\nax.add_collection(lc)\n");
            }
            if (e.segs.size()==1) fprintf(file,"circ=plt.Circle((%f,%f),%f,fill=True,linewidth=0.1,alpha=%f,edgecolor='none',fc='%c')\nfig.gca().add_artist(circ)\n",e.segs[0].f,e.segs[0].s,e.radplot,e.transparency,e.color);
        }
        else {
            if (e.color!='w') fprintf(file,"circ=plt.Circle((%f,%f),%f,fill=False,linewidth=0.2,color='%c')\nfig.gca().add_artist(circ)\n",e.O.f,e.O.s,e.rad,e.color);
            else fprintf(file,"circ=plt.Circle((%f,%f),%f,fill=False,linewidth=0.2,color='k',alpha=0.4)\nfig.gca().add_artist(circ)\n",e.O.f,e.O.s,e.rad);
        }
    }

    // Plot text
    if (text.size()!=textpos.size()) printf("Error sizes pyplot text\n");
    else for (int i=0; i<text.size(); i++) fprintf(file,"plt.text(%f,%f,'%s',horizontalalignment='center',verticalalignment='center',size=1)\n\n",textpos[i].f,textpos[i].s,text[i].c_str());
    
    // Finally
    fprintf(file,"\nfig.savefig('tile%d.pdf',bbox_inches='tight',pad_inches=0,dpi=(300))",j);
    fclose(file);
}
