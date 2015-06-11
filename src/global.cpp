#include	<cstdlib>
#include	<cmath>
#include	<fstream>
#include	<sstream>
#include	<iostream>
#include	<iomanip>
#include	<string>
#include        <string.h>
#include	<vector>
#include	<algorithm>
#include	<exception>
#include	<sys/time.h>
#include        <stdlib.h>     /* srand, rand */
#include	"modules/htmTree.h"
#include	"modules/kdTree.h"
#include        "omp.h"
#include        "misc.h"
#include        "feat.h"
#include        "structs.h"
#include        "global.h"

// Collecting information from input -------------------------------------------------------------------------------------
// Fast because no big function is called ! Keep it in only 1 function
// For each ~10,000 plate, finds ~25,000 galaxies reachable by the plate,
// Projects them, and for each fiber, finds reachable ones
void collect_galaxies_for_all(const Gals& G, const htmTree<struct galaxy>& T, Plates& P, const PP& pp, const Feat& F) {
	Time t;
	init_time(t,"# Begin collecting available galaxies");
	List permut = random_permut(F.Nplate);
	double rad = F.PlateRadius*M_PI/180.;
	int jj;
	//omp_set_num_threads(24);
#pragma omp parallel
	{ 	int id = omp_get_thread_num(); if (id==0) printf(" ");
		// Collects for each plate ; shuffle order of plates (useless !?)
		for (jj=id; jj<F.Nplate; jj++) { // <- begins at id, otherwise all begin at 0 -> conflict. Do all plates anyway
			int j = permut[jj];
			plate p = P[j];
			// Takes neighboring ~25000 galaxies that can be reached by this plate
			std::vector<int> nbr = T.near(G,p.nhat,rad); // nbr for neighbours
			// Projects thoses galaxies on the focal plane
			Onplates O;
			for (int gg=0; gg<nbr.size(); gg++) {
				int g = nbr[gg];
				struct onplate op = change_coords(G[g],p); 
				op.id = g;
				O.push_back(op);
			}
			printf(""); // Management of memory works better this way, and this function doesn't crash then
			KDtree<struct onplate> kdT(O,2);
			// For each fiber, finds all reachable galaxies
			for (int k=0; k<F.Nfiber; k++) {
				dpair X = pp.coords(k);
				std::vector<int> gals = kdT.near(&(pp.fp[2*k]),0.0,F.PatrolRad);
				for (int g=0; g<gals.size(); g++) {
					dpair Xg = projection(gals[g],j,G,P);
					if (sq(Xg,X)<sq(F.PatrolRad)/*Needed*/) P[j].av_gals[k].push_back(gals[g]);
				}
			}
		}
	} // End parallel
	print_time(t,"# ... took :");
}

void collect_available_tilefibers(Gals& G, const Plates& P, const Feat& F) {
	Time t;
	init_time(t,"# Begin computing available tilefibers");
	for(int j=0; j<F.Nplate; j++) {
		for(int k=0; k<F.Nfiber; k++) {
			for(int m=0; m<P[j].av_gals[k].size(); m++) {
				int i = P[j].av_gals[k][m];
				G[i].av_tfs.push_back(pair(j,k));
			}
		}
	}
	print_time(t,"# ... took :");
}

void init_densities(const Gals& G, Plates& P, const Feat& F) {
	for (int j=0; j<F.Nplate; j++) {
		for (int k=0; k<F.Nfiber; k++) {
			int cnt = 0;
			for (int i=0; i<P[j].av_gals[k].size(); i++) {
				int g = P[j].av_gals[k][i];
				int id = G[g].id;
				if (isfound(id,F.no_ss_sf)) cnt += /*A.nobs_time(g,j,G,F)*/ F.goal[id];
			}
			P[j].density[k] = cnt;
		}
	}
}

inline bool ok_assign_g_to_jk(int g, int j, int k, const Plates& P, const Gals& G, const PP& pp, const Feat& F, const Assignment& A) { // The order of tests matters for computation time
	int kind = G[g].id;
	if (kind==F.ids.at("SS") || kind==F.ids.at("SF")) return false;
	if (P[j].ipass==F.Npass-1 && kind!=F.ids.at("ELG")) return false; // Only ELG, SF, SS at the last pass
	if (A.unused[j][pp.spectrom[k]]<=0) return false;
	if (F.Collision) for (int i=0; i<pp.N[k].size(); i++) if (g==A.TF[j][pp.N[k][i]]) return false; // Avoid that 2 neighboring fibers observe the same galaxy
	if (A.find_collision(j,k,g,pp,G,P,F)!=-1) return false;
	return true;
}

// Assignment sub-functions -------------------------------------------------------------------------------------
// There is not problem with the fact that we only have knowledge on previous assignment, because when we call nobs (the only moment it can raise a problem) there can't be crossings with this plate, because g can only be assigned once
// Be very careful of (j,k) calling this function in other ! (if improvement function doesn't work anymore it's likely that bad argu,ent where called
inline int find_best(int j, int k, const Gals& G, const Plates& P, const PP& pp, const Feat& F, const Assignment& A, bool has_tf=false, int no_g=-1, List kind=Null()) { // ! Null list mean everything
	int best = -1; int mbest = -1; int pbest = 1e3;
	List av_gals = P[j].av_gals[k];
	for (int gg=0; gg<av_gals.size(); gg++) {
		int g = av_gals[gg];
		int m = A.nobs(g,G,F);
		if (m>=1) { // Less neat to compute it here but optimizes. Since there are a lot of SS and SF, it's worth putting ok_assign_ss_sf here
			int prio = fprio(g,G,F,A);
			if (prio<pbest || (prio==pbest && m>mbest)) { // Less neat to compute it here but optimizes
				bool tfb = has_tf ? !A.is_assigned_tf(j,k) : true;
				if (tfb && A.is_assigned_jg(j,g,G,F)==-1 && ok_assign_g_to_jk(g,j,k,P,G,pp,F,A) && g!=no_g && (kind.size()==0 || isfound(G[g].id,kind))) {
					best = g;
					pbest = prio;
					mbest = m;
				}
			}
		}
	}
	return best;
}

inline int assign_fiber(int j, int k, const Gals& G, const Plates& P, const PP& pp, const Feat& F, Assignment& A, int no_g=-1, List kind=Null()) {
	int best = find_best(j,k,G,P,pp,F,A,true,no_g,kind);
	//if ((j==2500 || j==1500 || j==5500) && best==-1 && P[j].av_gals[k].size()!=0) {
		//List av_g = P[j].av_gals[k];
		//for (int i=0; i<av_g.size(); i++) {
			//int g = av_g[i];
			//int kind0 = G[g].id;
			//if (A.is_assigned_jg(j,g)==-1 && 1<=A.nobs(g,G,F) && kind0!=7 && kind0!=6 && (kind.size()==0 || isfound(kind0,kind))) { printf("%d %s %d %d %d %d - ",k,F.kind[kind0].c_str(),A.is_assigned_jg(j,g,G,F)==-1,A.find_collision(j,k,g,pp,G,P,F)==-1,ok_assign_g_to_jk(g,j,k,P,G,pp,F,A),j);
			//}
		//}
	//}
	if (best!=-1) A.assign(j,k,best,G,P,pp);
	return best;
}

inline void assign_galaxy(int g, const Gals& G, const Plates& P, const PP& pp, const Feat& F, Assignment& A, int j00=-1, int size=-1) {
	int j0 = (j00==-1) ? A.next_plate : j00;
	int n = (size==-1) ? F.Nplate-j0 : size;
	int jb = -1; int kb = -1; int unusedb = -1;
	Plist av_tfs = G[g].av_tfs;
	for (int tfs=0; tfs<av_tfs.size(); tfs++) {
		int j = av_tfs[tfs].f;
		int k = av_tfs[tfs].s;
		if (j0<j && j<j0+n && !A.is_assigned_tf(j,k) && ok_assign_g_to_jk(g,j,k,P,G,pp,F,A)) {
			int unused = A.unused[j][pp.spectrom[k]];
			if (unusedb<unused) {
				jb = j; kb = k; unusedb = unused;
			}
		}
	}
	if (jb!=-1) A.assign(jb,kb,g,G,P,pp);
}

inline int assign_fiber_to_ss_sf(int j, int k, const Gals& G, const Plates& P, const PP& pp, const Feat& F, Assignment& A) {
	int best = -1; int pbest = 1e3;
	List av_gals = P[j].av_gals[k];
	for (int gg=0; gg<av_gals.size(); gg++) {
		int g = av_gals[gg];
		int kind = G[g].id;
		int prio = fprio(g,G,F,A);
		if (((kind==F.ids.at("SF") && A.nkind(j,k,F.ids.at("SF"),G,P,pp,F)<F.MaxSF) || (kind==F.ids.at("SS") && A.nkind(j,k,F.ids.at("SS"),G,P,pp,F)<F.MaxSS)) && prio<pbest) { // Less neat to compute it here but optimizes
			if (A.is_assigned_jg(j,g,G,F)==-1 && A.find_collision(j,k,g,pp,G,P,F)==-1) {
				best = g;
				pbest = prio;
			}
		}
	}
	if (best!=-1) A.assign(j,k,best,G,P,pp);
	return best;
}

// Try to use unused fibers by reassigning some used ones
// We could improve this function by taking best amongst all possible improvements !
inline int improve_fiber(int begin, int next, int j, int k, const Gals& G, const Plates& P, const PP& pp, const Feat& F, Assignment& A, int no_g=-1) {
	if (!A.is_assigned_tf(j,k)) { // Unused tilefiber (j,k)
		int g_try = assign_fiber(j,k,G,P,pp,F,A,no_g); // Doesn't add anything in practice, if there was an assignment first, though it can be useful in other cases
		if (g_try!=-1) return g_try;
		else { // Improve
			int gb = -1; int bb = -1; int jpb = -1; int kpb = -1; int mb = -1; int pb = 1e3; int unusedb = -1;
			List av_g = P[j].av_gals[k];
			for (int i=0; i<av_g.size(); i++) {
				int g = av_g[i]; // g : possible galaxy for (j,k)
				if (g!=-1 && g!=no_g) {
					// Is it allowed for jk to take g ?
					if (ok_assign_g_to_jk(g,j,k,P,G,pp,F,A)) {
						// What tfs have taken g ? Could they take someone else ?
						Plist tfs = A.chosen_tfs(g,F,begin,next);
						for (int p=0; p<tfs.size(); p++) {
							int jp = tfs[p].f;
							int kp = tfs[p].s; // (jp,kp) currently assigned to galaxy g
							// FIND BEST JP KP !!!
							int best = find_best(jp,kp,G,P,pp,F,A,false,-1,Null()); // best!=g because !A.assigned_pg(best)

							if (best!=-1 && (A.is_assigned_jg(j,g,G,F)==-1 || jp==j)) {
								int prio = fprio(best,G,F,A);
								int m = A.nobs(best,G,F);
								int unused = A.unused[jp][pp.spectrom[kp]]; // We take the most unused
								if (prio<pb || (prio==pb && m>mb) || (prio==pb && m==mb && unused>unusedb)) {
									gb = g; bb = best; jpb = jp; kpb = kp; mb = m; pb = prio; unusedb = unused;
							}}}}}}
			// Modify assignment
			if (gb!=-1) {
				A.unassign(jpb,kpb,gb,G,P,pp);
				A.assign(j,k,gb,G,P,pp);
				A.assign(jpb,kpb,bb,G,P,pp);
				return gb;
			}
		}
	}
	return -1;
}

int improve_fiber_from_kind(int id, int j, int k, const Gals& G, const Plates&P, const PP& pp, const Feat& F, Assignment& A) {
	if (!A.is_assigned_tf(j,k)) { // Unused tilefiber (j,k)
		int p = pp.spectrom[k];
		List fibskind = A.fibs_of_kind(id,j,p,G,pp,F);
		List no_kind; no_kind.push_back(id);
		int gb = -1; int gpb = -1; int bb = -1; int kpb = -1; int mb = -1; int pb = 1e3;
		List av_gals = P[j].av_gals[k];
		for (int i=0; i<av_gals.size(); i++) {
			int g = av_gals[i];
			if (G[g].id==id && A.find_collision(j,k,g,pp,G,P,F)==-1 && A.is_assigned_jg(j,g,G,F)==-1 && A.nobs(g,G,F)>=1 && 0<A.unused[j][pp.spectrom[k]]) {
				for (int kkp=0; kkp<fibskind.size(); kkp++) {
					int kp = fibskind[kkp];
					int gp = A.TF[j][kp];
					int best = find_best(j,kp,G,P,pp,F,A,false,-1,no_kind);
					if (best!=-1) {
						int prio = fprio(best,G,F,A);
						int m = A.nobs(best,G,F);
						if (prio<pb || (prio==pb && m>mb)) {
							if (!A.find_collision(j,k,kp,g,best,pp,G,P,F)) { // Avoid that the choice of the 2 new objects collide
							gb = g; gpb = gp; bb = best; kpb = kp; mb = m; pb = prio;
		}}}}}}
		// Modify assignment
		if (gb!=-1) {
			A.unassign(j,kpb,gpb,G,P,pp);
			A.assign(j,k,gb,G,P,pp);
			A.assign(j,kpb,bb,G,P,pp);
			return gb;
		}
	}
	return -1;
}

// Assignment functions ------------------------------------------------------------------------------------------
// Assign fibers naively
void simple_assign(const Gals& G, const Plates& P, const PP& pp, const Feat& F, Assignment& A, int next) {
	Time t;
	if (next!=1) init_time(t,"# Begin simple assignment :");
	int j0 = A.next_plate;
	int n = next==-1 ? F.Nplate-j0 : next; // Not F.Nplate-A.next_plate+1
	List plates = sublist(j0,n,A.order);
	List randPlates = F.Randomize ? random_permut(plates) : plates;
	for (int jj=0; jj<n; jj++) {
		int j = randPlates[jj];
		List randFibers = random_permut(F.Nfiber);
		for (int kk=0; kk<F.Nfiber; kk++) { // Fiber
			int k = randFibers[kk];
			assign_fiber(j,k,G,P,pp,F,A);
		}
	}
	str next_str = next==-1 ? "all left" : f(n);
	printf("  %s assignments on %s next plates\n",f(A.na(F,j0,n)).c_str(),next_str.c_str());
	if (next!=1) print_time(t,"# ... took :");
}

void improve(const Gals& G, const Plates&P, const PP& pp, const Feat& F, Assignment& A, int next) {
	Time t;
	if (next!=1) init_time(t,"# Begin improve :");
	int j0 = A.next_plate;
	int n = next==-1 ? F.Nplate-j0 : next;
	int na_start = A.na(F,j0,n);
	List plates = sublist(j0,n,A.order);
	List randPlates = F.Randomize ? random_permut(plates) : plates;
	for (int jj=0; jj<n; jj++) for (int k=0; k<F.Nfiber; k++) improve_fiber(j0,n,randPlates[jj],k,G,P,pp,F,A);
	int na_end = A.na(F,j0,n);
	printf("  %s more assignments (%.3f %% improvement)\n",f(na_end-na_start).c_str(),percent(na_end-na_start,na_start));
	if (next!=1) print_time(t,"# ... took :");
}

void improve_from_kind(const Gals& G, const Plates&P, const PP& pp, const Feat& F, Assignment& A, str kind, int next) {
	Time t;
	if (next!=1) init_time(t,"# Begin improve "+kind+" :");
	int id = F.ids.at(kind);
	List no_kind; no_kind.push_back(id);
	int j0 = A.next_plate;
	int n = next==-1 ? F.Nplate-A.next_plate : next;
	int na_start(A.na(F,j0,n));
	List plates = sublist(j0,n,A.order);
	List randPlates = F.Randomize ? random_permut(plates) : plates;
	for (int jj=0; jj<n; jj++) {
		int j = randPlates[jj];
		List randPetals = random_permut(F.Npetal);
		for (int ppet=0; ppet<F.Npetal; ppet++) {
			int p = randPetals[ppet];
			// Take sublist of fibers assigned to kind, and unassigned ones
			List fibskind = A.fibs_of_kind(id,j,p,G,pp,F);
			List fibsunas = A.fibs_unassigned(j,p,G,pp,F);
			//for (int kk=0; kk<fibsunas.size(); kk++) {
				 //gp = improve_fiber_from_kind(F.ids.at(kind),jp,kp,G,P,pp,F,A);
			//}
			for (int kk=0; kk<fibsunas.size(); kk++) {
				// Take an unassigned tf and its available galaxies
				int k = fibsunas[kk];
				int gb = -1; int gpb = -1; int bb = -1; int kpb = -1; int kkpb = -1; int mb = -1; int pb = 1e3;
				List av_gals = P[j].av_gals[k];
				for (int i=0; i<av_gals.size(); i++) {
					int g = av_gals[i];
					if (G[g].id==id && A.nobs(g,G,F)>=1 && A.is_assigned_jg(j,g,G,F)==-1 && A.find_collision(j,k,g,pp,G,P,F)==-1 && 0<A.unused[j][pp.spectrom[k]]) {
						// If the av gal is of the good kind, try to assign it, and improving an other one
						for (int kkp=0; kkp<fibskind.size(); kkp++) {
							int kp = fibskind[kkp];
							int gp = A.TF[j][kp];
							int best = find_best(j,kp,G,P,pp,F,A,false,-1,no_kind);
							if (best!=-1) {
								int prio = fprio(best,G,F,A);
								int m = A.nobs(best,G,F);
								if (prio<pb || prio==pb && m>mb) {
									if (!A.find_collision(j,k,kp,g,best,pp,G,P,F)) { // Avoid that the choice of the 2 new objects collide
									gb = g; gpb = gp; bb = best; kpb = kp; kkpb = kkp; mb = m; pb = prio;
							}}}
							else {
								erase(kkp,fibskind); // Don't try to change this one anymore because it will always be -1
								kkp--;
							}
						}}}
			// Modify assignment
				if (gb!=-1) {
					A.unassign(j,kpb,gpb,G,P,pp);
					A.assign(j,k,gb,G,P,pp);
					A.assign(j,kpb,bb,G,P,pp);
					erase(kkpb,fibskind);
				}
			}
		}
	}
	int na_end(A.na(F,j0,n));
	if (next==1) printf(" %s-imp +%3s (+%.3f %%)",kind.c_str(),f(na_end-na_start).c_str(),percent(na_end-na_start,na_start));
	else {
		printf("  %s more assignments (%.3f %% improvement)\n",f(na_end-na_start).c_str(),percent(na_end-na_start,na_start));
		print_time(t,"# ... took :");
	}
}

// If there are galaxies discovered as fake for example, they won't be observed several times in the plan
void update_plan_from_one_obs(const Gals& G, const Plates&P, const PP& pp, const Feat& F, Assignment& A, int end) {
	int cnt(0);
	int j0 = A.next_plate;
	int jpast = j0-F.Analysis;
	if (jpast<0) { printf("ERROR in update : jpast negative\n"); fl(); }
	int n = end-j0+1;
	int na_start(A.na(F,j0,n));
	List to_update;
	// Declare that we've seen those galaxies
	A.update_once_obs(jpast,F);
	// Get the list of galaxies to update in the plan
	for (int k=0; k<F.Nfiber; k++) {
		int g = A.TF[jpast][k];
		// Only if once_obs, we delete all further assignment. obs!=obs_tmp means that the galaxy is a fake one for example (same priority but different goal)
		if (g!=-1 && A.nobsv_tmp[g]!=A.nobsv[g] && A.once_obs[g]) to_update.push_back(g);
	}
	// Update information on previously seen galaxies
	A.update_nobsv_tmp_for_one(jpast,F);
	// Update further in the plan
	for (int gg=0; gg<to_update.size(); gg++) {
		int g = to_update[gg];
		Plist tfs = A.chosen_tfs(g,F,j0+1,n-1); // Begin at j0+1, because we can't change assignment at j0 (already watched)
		while (tfs.size()!=0) {
			int jp = tfs[0].f; int kp = tfs[0].s;
			//print_Plist(tfs,"Before"); // Debug
			A.unassign(jp,kp,g,G,P,pp);
			int gp = -1;
			//debl(1);
			gp = improve_fiber(j0+1,n-1,jp,kp,G,P,pp,F,A,g);
			//debl(2);
			//if (gp==-1) gp = improve_fiber_from_kind(F.ids.at("SF"),jp,kp,G,P,pp,F,A);
			//if (gp==-1) {debl(3); gp = improve_fiber_from_kind(F.ids.at("SF"),jp,kp,G,P,pp,F,A); debl(gp);}
			//if (gp==-1) gp = improve_fiber_from_kind(F.ids.at("SS"),jp,kp,G,P,pp,F,A);
			//if (gp==-1) {debl(5); gp = improve_fiber_from_kind(F.ids.at("SS"),jp,kp,G,P,pp,F,A); debl(gp);}
			//debl(7);
			erase(0,tfs);
			//debl(8);
			//print_Plist(tfs,"After"); // Debug
			cnt++;
		}
	}
	int na_end(A.na(F,j0,n));
	printf(" %4d unas & %4d replaced\n",cnt,na_end-na_start+cnt); fl();
}

// If no enough SS and SF, remove old_kind an replace to SS-SF (new_kind) on petal (j,p)
void replace(List old_kind, int new_kind, int j, int p, const Gals& G, const Plates& P, const PP& pp, const Feat& F, Assignment& A) {
	int m = A.nkind(j,p,new_kind,G,P,pp,F,true);
	List fibskindd;
	for (int i=0; i<old_kind.size(); i++) addlist(fibskindd,A.fibs_of_kind(old_kind[i],j,p,G,pp,F));
	List fibskind0 = random_permut(fibskindd);
	List fibskind = A.sort_fibs_dens(j,fibskind0,G,P,pp,F);
	int Max = new_kind==F.ids.at("SS") ? F.MaxSS : F.MaxSF;
	while (m<Max && fibskind.size()!=0) {
		bool fin(false);
		int k = fibskind[0];
		List av_g = P[j].av_gals[k];
		for (int gg=0; gg<av_g.size() && !fin; gg++) {
			int g = av_g[gg];
			if (G[g].id==new_kind && A.find_collision(j,k,g,pp,G,P,F)==-1 /*A.nobs(g,G,F)>=1*/) {
				int g0 = A.TF[j][k];
				A.unassign(j,k,g0,G,P,pp);
				//assign_galaxy(g0,G,P,pp,F,A);
				A.assign(j,k,g,G,P,pp);
				fin = true;
				m++;
			}
		}
		erase(0,fibskind);
	}
}

void assign_left(int j, const Gals& G, const Plates& P, const PP& pp, const Feat& F, Assignment& A) { // Tries to assign left fibers, even taking objects further observed
	for (int k=0; k<F.Nfiber; k++) {
		if (!A.is_assigned_tf(j,k)) {
			int best = -1; int mbest = -1; int pbest = 1e3; int jpb = -1; int kpb = -1;
			List av_gals = P[j].av_gals[k];
			for (int gg=0; gg<av_gals.size(); gg++) {
				int g = av_gals[gg];
				int m = A.nobs(g,G,F);
				int prio = fprio(g,G,F,A);
				if (prio<pbest || (prio==pbest && m>mbest)) { // Less neat to compute it here but optimizes
					if (A.is_assigned_jg(j,g,G,F)==-1 && ok_assign_g_to_jk(g,j,k,P,G,pp,F,A)) {
						for (int i=0; i<A.GL[g].size(); i++) {
							int jp = A.GL[g][i].f;
							int kp = A.GL[g][i].s;
							if (j<jp && jpb<jp) {
								best = g;
								pbest = prio;
								mbest = m;
								jpb = jp;
								kpb = kp;
							}
						}
					}
				}
			}
			if (best!=-1) {
				A.unassign(jpb,kpb,best,G,P,pp);
				A.assign(j,k,best,G,P,pp);
			}
		}
	}
}

// If no enough SS and SF, remove old_kind an replace to SS-SF (new_kind) on petal (j,p)
void assign_sf_ss(int j, const Gals& G, const Plates& P, const PP& pp, const Feat& F, Assignment& A) {
	str lrgA[] = {"LRG","FakeLRG"}; List lrg = F.init_ids_list(lrgA,2);
	str elgA[] = {"ELG"}; List elg = F.init_ids_list(elgA,1);
	List randPetals = random_permut(F.Npetal);
	for (int ppet=0; ppet<F.Npetal; ppet++) {
		int p = randPetals[ppet];
		List randFibers = random_permut(pp.fibers_of_sp[p]);
		if (!F.InfDens) {
			// Assign SS-SF
			for (int kk=0; kk<F.Nfbp; kk++) {
				int k = randFibers[kk];
				if (!A.is_assigned_tf(j,k)) assign_fiber_to_ss_sf(j,k,G,P,pp,F,A);
			}
			// If not enough SS and SF, remove ELG an replace to SS-SF
			replace(elg,F.ids.at("SS"),j,p,G,P,pp,F,A);
			replace(elg,F.ids.at("SF"),j,p,G,P,pp,F,A);
			replace(lrg,F.ids.at("SS"),j,p,G,P,pp,F,A);
			replace(lrg,F.ids.at("SF"),j,p,G,P,pp,F,A);
			if (A.kinds[j][p][F.ids.at("SS")]!=F.MaxSS) printf("! Not enough SS !\n");
			if (A.kinds[j][p][F.ids.at("SF")]!=F.MaxSF) printf("! Not enough SF !\n");
		}
		else {
			List elgs = A.fibs_of_kind(F.ids.at("ELG"),j,p,G,pp,F);
			int unused = A.unused[j][p];
			for (int kk=0; kk<elgs.size() && unused<F.MaxSS+F.MaxSF; kk++) {
				int k = elgs[kk];
				A.unassign(j,k,A.TF[j][k],G,P,pp);
				unused++;
			}
			if (unused<F.MaxSS+F.MaxSF) printf("! Not enough !\n");
		}
	}
}

// For each petal, assign QSOs, LRGs, ELGs, ignoring SS and SF. Then if there are free fibers, try to assign them first to SS and then SF. Now if we don't have 10 SS and 40 SF in a petal, take SS and SF at random from those that are available to the petal and if their fiber is assigned to an ELG, remove that assignment and give it instead to the SS or SF.
void new_assign_fibers(const Gals& G, const Plates& P, const PP& pp, const Feat& F, Assignment& A, int next) {
	Time t;
	if (next!=1) init_time(t,"# Begin new assignment :\n-------------------------------------------------------------");
	int j0 = A.next_plate;
	int n = next==-1 ? F.Nplate-j0 : next; // Not F.Nplate-A.next_plate+1
	List plates = sublist(j0,n,A.order);
	List randPlates = F.Randomize ? random_permut(plates) : plates;

	str qsoA[] = {"QSOLy-a","QSOTracer","FakeQSO"}; List qso = F.init_ids_list(qsoA,3);
	str lrgA[] = {"LRG","FakeLRG"}; List lrg = F.init_ids_list(lrgA,2);
	str elgA[] = {"ELG"}; List elg = F.init_ids_list(elgA,1);
	str ss_sfA[] = {"SS","SF"}; List ss_sf = F.init_ids_list(ss_sfA,2);
	for (int jj=0; jj<n; jj++) {
		//if (jj==floor(n/4)) print_time("1/4 of total tiles processed in \n");
		if (jj%(int)floor(n/60)==0) { printf("-"); fl(); }
		int j = randPlates[jj];
		List randPetals = random_permut(F.Npetal);
		for (int ppet=0; ppet<F.Npetal; ppet++) {
			int p = randPetals[ppet];
			List randFibers = random_permut(pp.fibers_of_sp[p]);
			// Assign QSO
			for (int kk=0; kk<F.Nfbp; kk++) {
				int k = randFibers[kk];
				assign_fiber(j,k,G,P,pp,F,A,-1,qso);
			}
			// Assign LRG
			for (int kk=0; kk<F.Nfbp; kk++) {
				int k = randFibers[kk];
				if (!A.is_assigned_tf(j,k)) assign_fiber(j,k,G,P,pp,F,A,-1,lrg);
			}
			// Assign ELG
			for (int kk=0; kk<F.Nfbp; kk++) {
				int k = randFibers[kk];
				if (!A.is_assigned_tf(j,k)) assign_fiber(j,k,G,P,pp,F,A,-1,elg);
			}
		}
	}
	str next_str = next==-1 ? "all left" : f(next);
	printf("\n  %s assignments on %s next plates\n",f(A.na(F,j0,n)).c_str(),next_str.c_str());
	if (next!=1) print_time(t,"# ... took :");
}

void redistribute_tf(const Gals& G, const Plates&P, const PP& pp, const Feat& F, Assignment& A, int next) {
	Time t;
	if (next!=1) init_time(t,"# Begin redistribute TF :");
	int j0 = A.next_plate;
	int n = next==-1 ? F.Nplate-A.next_plate : next;
	List plates = sublist(j0,n,A.order);
	//List randPlates = F.Randomize ? random_permut(plates) : plates;
	List randPlates = random_permut(plates);
	int red(0);
	Table Done = initTable(F.Nplate,F.Nfiber);
	for (int jj=0; jj<n; jj++) {
		int j = randPlates[jj];
		List randFiber = random_permut(F.Nfiber);
		for (int kk=0; kk<F.Nfiber; kk++) {
			int k = randFiber[kk];
			if (Done[j][k]==0) {
				int g = A.TF[j][k];
				if (g!=-1) {
					int jpb = -1; int kpb = -1; int unusedb = A.unused[j][pp.spectrom[k]];
					Plist av_tfs = G[g].av_tfs;
					for (int i=0; i<av_tfs.size(); i++) {
						int jp = av_tfs[i].f;
						int kp = av_tfs[i].s;
						int unused = A.unused[jp][pp.spectrom[kp]];
						if (j0<=jp && jp<j0+n && !A.is_assigned_tf(jp,kp) && Done[jp][kp]==0 && ok_assign_g_to_jk(g,jp,kp,P,G,pp,F,A) && A.is_assigned_jg(jp,g,G,F)==-1 && 0<unused) {
							if (unusedb<unused) { // Takes the most usused petal
								jpb = jp;
								kpb = kp;
								unusedb = unused;
							}
						}
					}
					if (jpb!=-1) {
						A.unassign(j,k,g,G,P,pp);
						A.assign(jpb,kpb,g,G,P,pp);
						Done[j][k] = 1;
						Done[jpb][kpb] = 1;
						red++; 
					}
				}
			}
		}
	}
	printf("  %s redistributions of couples of TF\n",f(red).c_str());
	if (next!=1) print_time(t,"# ... took :");
}

// Other useful functions --------------------------------------------------------------------------------------------
void results_on_inputs(str outdir, const Gals& G, const Plates& P, const Feat& F, bool latex) {
	printf("# Results on inputs :\n");
	// Print features
	//print_list("  Kinds corresponding :",F.kind);
	//print_list("  Priorities :",F.prio);
	//print_list("  Goals of observations :",F.goal);
	//print_list("  Max goals of observations :",F.maxgoal());

	// How many galaxies in range of a fiber ?
	List data;
	for (int j=0; j<F.Nplate; j++) for (int k=0; k<F.Nfiber; k++) data.push_back(P[j].av_gals[k].size());
	print_list("  How many galaxies in range of a fiber :",histogram(data,1));

	// 1 Histograms on number of av gals per plate and per fiber
	Cube T = initCube(F.Categories,F.Nplate,F.Nfiber);
	for (int j=0; j<F.Nplate; j++) {
		for (int k=0; k<F.Nfiber; k++) {
			List gals = P[j].av_gals[k];
			for (int g=0; g<gals.size(); g++) T[G[gals[g]].id][j][k]++;
		}
	}
	Table hist1;
	for (int id=0; id<F.Categories; id++) hist1.push_back(histogram(T[id],1));
	print_mult_table_latex("Available galaxies (by kind) for a TF",outdir+"avgalhist.dat",hist1,1);

	// 2 Histograms on number of av tfs per galaxy
	Table Tg = initTable(F.Categories,0);
	for (int g=0; g<F.Ngal; g++) {
		int n = G[g].av_tfs.size();
		Tg[G[g].id].push_back(n);
	}
	Table hist2;
	for (int id=0; id<F.Categories; id++) hist2.push_back(histogram(Tg[id],1));
	print_mult_table_latex("Available tile-fibers for a galaxy (by kind)",outdir+"avtfhist.dat",hist2,1);

	// 3 Histogram of number of times (by different plates) reachable galaxies
	List countgals;
	List countgals_nopass;
	for (int g=0; g<F.Ngal; g++) {
		int id = G[g].id;
		List plates;
		for (int i=0; i<G[g].av_tfs.size(); i++) {
			int j = G[g].av_tfs[i].f;
			if (!isfound(j,plates) && P[j].ipass!=F.Npass-1) plates.push_back(j);
		}
		countgals.push_back(plates.size());
	}
	Dtable countstot;
	List h00 = histogram(countgals,1);
	countstot.push_back(percents(h00,sumlist(h00))); 
	print_mult_Dtable_latex("Histogram of percents (by different plates) reachable galaxies (not 5th pass)",outdir+"reachplate.dat",countstot,1);
	
	// By kind
	//Table countgals = initTable(F.Categories,0);
	//Table countgals_nopass = initTable(F.Categories,0);
	//for (int g=0; g<F.Ngal; g++) {
		//int id = G[g].id;
		//List plates; List plates_no;
		//for (int i=0; i<G[g].av_tfs.size(); i++) {
			//int j = G[g].av_tfs[i].f;
			//if (!isfound(j,plates)) {
				//plates.push_back(j);
				//if (P[j].ipass!=F.Npass-1 || F.id("ELG")==id) plates_no.push_back(j);
			//}
		//}
		//countgals[id].push_back(plates.size());
		//countgals_nopass[id].push_back(plates_no.size());
	//}
	//Table countstot; 
	//Table countstot_nopass; 
	//for (int i=0; i<F.Categories; i++) {
		//countstot.push_back(histogram(countgals[i],1)); 
		//countstot_nopass.push_back(histogram(countgals_nopass[i],1));
	//}
	//print_mult_table_latex("Histogram of number of times (by different plates) reachable galaxies",outdir+"reachplate.dat",countstot,1);
	//print_mult_table_latex("Histogram of number of times (by different plates) reachable galaxies without last pass",outdir+"reachplateno.dat",countstot_nopass,1);
	
	// 4 Histogram of redshifts
	Dtable countsz = initDtable(3,0);
	double intervalz = 0.02;
	for (int g=0; g<F.Ngal; g++) {
		int kind = G[g].id;
		int kind0 = -1;
		if (kind==F.id("QSOLy-a") || kind==F.id("QSOTracer") || kind==F.id("FakeQSO")) kind0 = 0;
		if (kind==F.id("LRG") || kind==F.id("FakeLRG")) kind0 = 1;
		if (kind==F.id("ELG")) kind0 = 2;
		if (kind0!=-1) countsz[kind0].push_back(G[g].z);
	}
	Dtable hist3;
	for (int id=0; id<3; id++) hist3.push_back(histogram(countsz[id],intervalz));
	print_mult_Dtable_latex("dn/dz",outdir+"redshifts.dat",hist3,intervalz);
}

void display_results(str outdir, const Gals& G, const Plates& P, const PP& pp, Feat& F, const Assignment& A, bool latex) {
	printf("# Results :\n");

	// 1 Raw numbers of galaxies by id and number of remaining observations
	int MaxObs = max(F.goal);
	Table obsrv = initTable(F.Categories,MaxObs+1);

	for (int g=0; g<F.Ngal; g++) {
		int id = G[g].id;
		int m = A.nobs(g,G,F,false);
		if (!(m>=0 && m<=MaxObs)) F.Count++;
		int n = F.goal[id]-m;
		if (n>=0 && n<=MaxObs) obsrv[id][n]++; // Some SS and SF are obs 6 or 7 times !
	}

	// Add the 3 columns of tot, fibs, obs
	Table with_tots = obsrv;
	for (int i=0; i<F.Categories; i++) {
		int fibs = 0; int obs = 0; int tot =0;
		for (int j=0; j<=MaxObs; j++) tot += obsrv[i][j];
		for (int j=0; j<=MaxObs; j++) fibs += obsrv[i][j]*j;
		for (int j=1; j<=MaxObs; j++) obs += obsrv[i][j];
		with_tots[i].push_back(tot);
		with_tots[i].push_back(fibs);
		with_tots[i].push_back(obs);
	}
	//print_table("  Remaining observations (without negative obs ones)",with_tots,latex,F.kind);
	Dtable obs_per_sqd = ddivide_floor(with_tots,F.TotalArea);

	// Add percentages of observation
	Dtable perc = initDtable(F.Categories,2);
	for (int id=0; id<F.Categories; id++) {
		int tot = sumlist(obsrv[id]);
		int goal = F.goal[id];

		perc[id][0] = percent(tot-obsrv[id][0],tot);

		// Weighted percentage
		int d = 0;
		for (int i=0; i<=goal; i++) d += obsrv[id][i]*i;
		perc[id][1] = percent(d,tot*goal);
	}
	print_table("Obs per sqd and percentages",concatenate(obs_per_sqd,perc),latex,F.kind);

	// 3 Observed galaxies in function of time
	// Lya 1,2,3,4,5, LRG 1,2
	if (F.PlotObsTime) {
	int interval = 10;
	int nk = 9;
	Table Ttim = initTable(nk,0);
	List galaxs = initList(F.Ngal);
	for (int j=0; j<F.Nplate; j++) {
		for (int k=0; k<F.Nfiber; k++) {
			int g = A.TF[j][k];
			if (g!=-1) galaxs[g]++;
		}
		if (j%interval==0) {
			List l = initList(9);
			for (int g=0; g<F.Ngal; g++) {
				int n = galaxs[g];
				if (1<=n) {
					if (G[g].id == 0) l[n-1]++;
					if (G[g].id == 2) l[n-1+5]++;
					if (G[g].id == 1) l[n+6]++;
					if (G[g].id == 3) l[n+7]++;
				}
			}
			for (int id=0; id<nk; id++) Ttim[id].push_back(l[id]);
		}
	}
	print_mult_table_latex("Observed galaxies complete (interval 10)",outdir+"time2.dat",Ttim,interval);
	}

	// 4 Histogram of percentages of seen Ly-a
	if (F.PlotHistLya) {
	int id = F.ids.at("QSOLy-a");
	int goal = F.goal[id];
	Table Percseen = initTable(goal+1,0);
	for (int g=0; g<F.Ngal; g++) {
		if (G[g].id==id) {
			int n = G[g].av_tfs.size();
			int p = A.chosen_tfs(g,F).size();
			if (n>=Percseen[p].size()) Percseen[p].resize(n+1);
			Percseen[p][n]++;
		}
	}
	make_square(Percseen);
	//print_table("Number of QSO Ly-a : x - Number of available TF - y - Number of observations",Percseen);
	for (int j=0; j<Percseen[0].size(); j++) {
		for (int i=Percseen.size()-1; i!=0; i--) {
			Percseen[i-1][j] += Percseen[i][j];
		}
	}
	print_mult_table_latex("Available tile-fibers for a galaxy (by kind)",outdir+"obsly.dat",Percseen,1);
	}

	// 5 Histogram of time between 2 obs of Ly a
	if (F.PlotDistLya) {
	Table deltas;
	for (int g=0; g<F.Ngal; g++) {
		if (G[g].id == F.ids.at("QSOLy-a")) {
			Plist tfs = A.chosen_tfs(g,F);
			if (tfs.size()>=2) {
				List unsorted;
				List del;
				for (int i=0; i<tfs.size(); i++) {
					unsorted.push_back(tfs[i].f);
				}
				List sorted = sort(unsorted);
				for (int i=0; i<sorted.size()-1; i++) {
					int p1 = sorted[i];
					int p2 = sorted[i+1];
					del.push_back(p2-p1);
				}
				deltas.push_back(del);
			}
		}
	}
	List histo0 = histogram(deltas,10);
	//print_hist("Plate interval between 2 consecutive obs of Ly-a (interval 100)",100,histogram(deltas,100));
	Table delts; delts.push_back(histo0); delts.push_back(cumulate(histo0));
	print_mult_table_latex("Plate interval between 2 consecutive obs of Ly-a (interval 10)",outdir+"dist2ly.dat",delts,10);
	}

	// 6 Free fibers histogram
	if (F.PlotFreeFibHist) {
	Table unused_fbp = A.unused_fbp(pp,F);
	make_square(unused_fbp);
	Table hist0; hist0.push_back(histogram(unused_fbp,1));
	print_mult_table_latex("Number of petals with this many free fiber (interval 1)",outdir+"freefib.dat",hist0,1);
	}

	// 7 Free fibers in function of time (plates)
	if (F.PlotFreeFibTime) {
	List freefibtime = initList(F.Nplate);
	for (int j=0; j<F.Nplate; j++) freefibtime[j] = A.unused_f(j,F);
	Table fft; fft.push_back(freefibtime);
	print_mult_table_latex("Free fibers in function of time (plates)",outdir+"fft.dat",fft);
	}
 
	// 8 Percentage of seen objects as a function of density of objects
	if (F.PlotSeenDens) {
	Dcube densities = initDcube(F.Categories+1,0,0);
	for (int j=0; j<F.Nplate; j++) {
		for (int k=0; k<F.Nfiber; k++) {
			// For all
			int size = P[j].av_gals[k].size();
			int oc = 0;
			for (int i=0; i<size; i++) if (A.is_assigned_jg(j,P[j].av_gals[k][i])!=-1) oc++;
			if (size!=0 && 1<=oc) { 
				double d = percent(oc,size);
				//printf("%f %f %f %d %d %d \n",d,x,invFibArea,size,oc,densities.size()); fl();
				if (size>=densities[F.Categories].size()) densities[F.Categories].resize(size+1);
				densities[F.Categories][size].push_back(d);
			}

			// For kind
			for (int t=0; t<F.Categories; t++) {
				int nkind = 0;
				int ock = 0;
				for (int i=0; i<size; i++) {
					int g = P[j].av_gals[k][i];
					if (G[g].id == t) {
						nkind++;
						if (A.is_assigned_jg(j,g)!=-1) ock++;
					}
				}
				if (nkind!=0 && 1<=ock) { 
					double d = percent(ock,nkind);
					if (nkind>=densities[t].size()) densities[t].resize(nkind+1);
					densities[t][nkind].push_back(d);
				}
			}
		}
	}
	Dtable densit = initDtable(F.Categories+1,max_row(densities));
	for (int t=0; t<F.Categories+1; t++) for (int i=0; i<densities[t].size(); i++) densit[t][i] = sumlist(densities[t][i])/densities[t][i].size();
	print_mult_Dtable_latex("Perc of seen obj as a fun of dens of objs",outdir+"seendens.dat",densit,1);
	}
	
	// 9 Collision histogram of distances between galaxies
	if (F.Collision) {
	Dlist coldist;
	for (int j=0; j<F.Nplate; j++) {
		List done = initList(F.Nfiber);
		for (int k=0; k<F.Nfiber; k++) {
			if (done[k]==0) {
				int c = A.is_collision(j,k,pp,G,P,F);
				if (c!=-1) {
					done[c] = 1;
					dpair G1 = projection(A.TF[j][k],j,G,P);
					dpair G2 = projection(A.TF[j][c],j,G,P);
					double d = norm(G2-G1);
					coldist.push_back(d);
				}
			}
		}
	}
	double intervaldist = 0.01;
	
	Dlist histcoldist = histogram(coldist,intervaldist);
	Dlist redhistcol = percents(histcoldist,sumlist(histcoldist));
	Dtable Dtd; Dtd.push_back(redhistcol); Dtd.push_back(cumulate(redhistcol));
	print_mult_Dtable_latex("Collision histogram of distances between galaxies",outdir+"coldist.dat",Dtd,intervaldist);
	}

	// Collision rate
	if (F.Collision) printf("Collision rate : %f \% \n",A.colrate(pp,G,P,F));

	// Percentage of fibers assigned
	printf("  %s assignments in total (%.4f %% of all fibers)\n",f(A.na(F)).c_str(),percent(A.na(F),F.Nplate*F.Nfiber));

	// Count
	if (F.Count!=0) printf("Count = %d \n",F.Count);
}

void write_FAtile_ascii(int j, str outdir, const Gals& G, const Plates& P, const PP& pp, const Feat& F, const Assignment& A) {
	FILE * FA;
	str s = outdir+"tile"+i2s(j)+".txt";
	FA = fopen(s.c_str(),"w");
	for (int k=0; k<F.Nfiber; k++) {
		int g = A.TF[j][k];
		// k
		fprintf(FA,"%d ",k);
		List av_gals = P[j].av_gals[k];
		// Number of potential galaxies
		fprintf(FA,"%d ",av_gals.size());
		// IDs of potential galaxies
		for (int i=0; i<av_gals.size(); i++) fprintf(FA,"%d ",av_gals[i]);
		// Object type, Target ID, ra, dec, x, y
		if (g!=-1) {
			dpair Gal = projection(g,j,G,P);
			fprintf(FA,"%s %d %f %f %f %f\n",F.kind[G[g].id].c_str(),g,G[g].ra,G[g].dec,Gal.f,Gal.s);
		}
		else fprintf(FA,"-1\n");
	}
	fclose(FA);
}
/*
void fa_write(int j, const char *filename, const Gals& G, const Plates& P, const PP& pp, const Feat& F, const Assignment& A) { // Lado Samushia
	int MAXTGT = 13;
	// initialize arrays
	int fiber_id[F.Nfiber];
	int positioner_id[F.Nfiber];
	int num_target[F.Nfiber];
	char objtype[F.Nfiber][8];
	char *ot_tmp[F.Nfiber];
	for (int i = 0; i < F.Nfiber; i++) ot_tmp[i] = objtype[i];
	int target_id[F.Nfiber];
	int desi_target[F.Nfiber];
	float ra[F.Nfiber];
	float dec[F.Nfiber];
	float x_focal[F.Nfiber];
	float y_focal[F.Nfiber];
	int potential_target_id[F.Nfiber*MAXTGT];
	int *ptid = potential_target_id;
	int tot_targets = 0;
	printf("start the loop\n");
	for (int i = j*F.Nfiber; i < F.Nfiber; i++) {
		int g = A.TF[j][i];
		int id = G[g].id;
		str type = F.type[id];
		//char type0[] = "1111111";


		fiber_id[i] = i;
		positioner_id[i] = i;
		num_target[i] = P[j].av_gals[i].size();
		printf("%d ", g);
		//objtype[i] = type0;
		strcpy(objtype[i], type.c_str());
		printf("%s ", objtype[i]);
		target_id[i] = g;
		desi_target[i] = 0;
		ra[i] = g == -1 ? 370.0 : G[g].ra;
		dec[i] = g == -1 ? 370.0 : G[g].dec;

		dpair proj = projection(g,j,G,P);
		x_focal[i] = proj.f;
		y_focal[i] = proj.s;

		for (int n = 0; n < P[j].av_gals[i].size(); n++) {
			*ptid = P[j].av_gals[i][n];
			ptid++;
		}
		tot_targets += P[j].av_gals[i].size();
	}
	// write to fits file
	int status;
	fitsfile *fptr;
	fits_create_file(&fptr, filename, &status);
	fits_report_error(stdout, status);
	// FiberMap table
	char *ttype[] = {"fiber", "positioner", "numtarget", "objtype", "targetid", "desi_target0", "ra", "dec", "xfocal_design", "yfocal_design"};
	char *tform[10] = {"U", "U", "U", "8A", "J", "K", "E", "E", "E", "E"};
	char *tunit[10] = { "", "", "", "", "", "", "deg", "deg", "mm", "mm"};
	char extname[] = "FiberMap";
	fits_create_tbl(fptr, BINARY_TBL, 0, 10, ttype, tform, tunit, extname, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TINT, 1, 1, 1, F.Nfiber, fiber_id, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TINT, 2, 1, 1, F.Nfiber, positioner_id, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TINT, 3, 1, 1, F.Nfiber, num_target, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TSTRING, 4, 1, 1, F.Nfiber, ot_tmp, &status);
	fits_report_error(stdout, status);
	fits_write_col(fptr, TINT, 5, 1, 1, F.Nfiber, target_id, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TINT, 6, 1, 1, F.Nfiber, desi_target, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TFLOAT, 7, 1, 1, F.Nfiber, ra, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TFLOAT, 8, 1, 1, F.Nfiber, dec, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TFLOAT, 9, 1, 1, F.Nfiber, x_focal, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TFLOAT, 10, 1, 1, F.Nfiber, y_focal, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	// PotentialFiberMap table
	char *ttype2[] = {"potentialtargetid"};
	char *tform2[1] = {"V"};
	char *tunit2[10] = {""};
	char extname2[] = "PotentialFiberMap";
	fits_create_tbl(fptr, BINARY_TBL, 0, 1, ttype2, tform2, tunit2, extname2, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_write_col(fptr, TINT, 1, 1, 1, tot_targets, potential_target_id, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	fits_close_file(fptr, &status);
	printf("#\n");
	fits_report_error(stdout, status);
	return;
}
*/
void pyplotTile(int j, str directory, const Gals& G, const Plates& P, const PP& pp, const Feat& F, const Assignment& A) {
	std::vector<char> colors;
	colors.resize(F.Categories);
	colors[0] = 'k'; colors[1] = 'g'; colors[2] = 'r'; colors[3] = 'b'; colors[4] = 'm'; colors[5] = 'y'; colors[6] = 'w'; colors[7] = 'c';
	polygon pol;
	PosP posp(3,3);
	for (int k=0; k<F.Nfiber; k++) {
		dpair O = pp.coords(k);
		int g = A.TF[j][k];
		if (g!=-1) {
			dpair Ga = projection(g,j,G,P);
			polygon fh = F.fh;
			polygon cb = F.cb;
			repos_cb_fh(cb,fh,O,Ga,posp);
			//if (A.is_collision(j,k,pp,G,P,F)!=-1) {
				//cb.set_color('r');
				//fh.set_color('r');
			//}
			cb.set_color(colors[G[g].id]);
			fh.set_color(colors[G[g].id]);
			pol.add(cb);
			pol.add(fh);
			pol.add(element(O,colors[G[g].id],0.3,5));
		}
		else pol.add(element(O,'k',0.1,3));
		List av_gals = P[j].av_gals[k];
		for (int i=0; i<av_gals.size(); i++) {
			int gg = av_gals[i];
			if (1<=A.nobs_time(gg,j,G,F)) {
				//if (A.nobs_time(gg,j,G,F)!=A.nobs(gg,G,F)) printf("%d %d %s - ",A.nobs_time(gg,j,G,F),A.nobs(gg,G,F),F.kind[G[gg].id].c_str());
				int kind = G[gg].id;
				dpair Ga = projection(gg,j,G,P);
				if (kind==F.ids.at("QSOLy-a")) pol.add(element(Ga,colors[kind],1,A.is_assigned_jg(j,gg)==-1?0.9:0.5));
				else pol.add(element(Ga,colors[kind],1,0.5));
			}
		}
	}
	pyplot pyp(pol);
	//for (int k=0; k<F.Nfiber; k++) pyp.addtext(pp.coords(k),i2s(k)); // Plot fibers identifiers
	pyp.plot_tile(directory,j,F); 
}

void overlappingTiles(str fname, const Feat& F, const Assignment& A) {
	FILE * file;
	file = fopen(fname.c_str(),"w");
	for (int g=0; g<F.Ngal; g++) {
		if (A.GL[g].size()==5) {
			fprintf(file,"%d ",g);
			for (int i=0; i<A.GL[g].size(); i++) fprintf(file,"(%d,%d) ",A.GL[g][i].f,A.GL[g][i].s);
			fprintf(file,"\n");
		}
	}
	fclose(file);
}

    //QSOLy-a   &     0 &     2 &   4 & 11 & 20 & 10 &    49 &   179 &    49 & 98.364 & 72.015 \\ 
  //QSOTracer   &     1 &   108 &   0 &  0 &  0 &  0 &   110 &   108 &   108 & 98.392 & 90.063 \\ 
        //LRG   &    18 &    37 & 242 &  0 &  0 &  0 &   298 &   523 &   280 & 93.884 & 87.570 \\ 
        //ELG   &   554 & 1,856 &   0 &  0 &  0 &  0 & 2,411 & 1,856 & 1,856 & 76.984 & 76.984 \\ 
    //FakeQSO   &     1 &    81 &   0 &  0 &  0 &  0 &    82 &    81 &    81 & 98.370 & 90.027 \\ 
    //FakeLRG   &     2 &    44 &   0 &  0 &  0 &  0 &    46 &    44 &    44 & 94.619 & 88.366 \\ 

// Interplate = Analysis = 0
    //QSOLy-a   &     0 &     2 &   4 & 11 & 20 & 10 &    49 &   180 &    49 & 98.333 & 72.259 \\ 
  //QSOTracer   &     1 &   118 &   0 &  0 &  0 &  0 &   119 &   118 &   118 & 98.350 & 98.350 \\ 
        //LRG   &    17 &    36 & 243 &  0 &  0 &  0 &   298 &   524 &   280 & 93.981 & 87.792 \\ 
        //ELG   &   546 & 1,865 &   0 &  0 &  0 &  0 & 2,411 & 1,865 & 1,865 & 77.351 & 77.351 \\ 
    //FakeQSO   &     1 &    88 &   0 &  0 &  0 &  0 &    90 &    88 &    88 & 98.344 & 98.344 \\ 
    //FakeLRG   &     2 &    47 &   0 &  0 &  0 &  0 &    50 &    47 &    47 & 94.681 & 94.681 \\ 
	 //
// Interplate = Analysis = 0 + new_assign fixed
    //QSOLy-a   &     0 &     1 &   3 & 10 & 21 & 11 &    49 &   185 &    49 & 98.398 & 74.097 \\ 
  //QSOTracer   &     1 &   118 &   0 &  0 &  0 &  0 &   119 &   118 &   118 & 98.404 & 98.404 \\ 
        //LRG   &    18 &    36 & 244 &  0 &  0 &  0 &   298 &   524 &   280 & 93.800 & 87.758 \\ 
        //ELG   &   547 & 1,864 &   0 &  0 &  0 &  0 & 2,411 & 1,864 & 1,864 & 77.308 & 77.308 \\ 
    //FakeQSO   &     1 &    88 &   0 &  0 &  0 &  0 &    90 &    88 &    88 & 98.412 & 98.412 \\ 
    //FakeLRG   &     2 &    47 &   0 &  0 &  0 &  0 &    50 &    47 &    47 & 94.501 & 94.501 \\ 

// Pareil avec truc des LRG fixed
    //QSOLy-a   &     0 &     1 &   3 & 10 & 21 & 11 &    49 &   185 &    49 &   98.4 & 74.106 \\ 
  //QSOTracer   &     1 &   118 &   0 &  0 &  0 &  0 &   119 &   118 &   118 & 98.407 & 98.407 \\ 
        //LRG   &    18 &    36 & 244 &  0 &  0 &  0 &   298 &   524 &   280 & 93.787 & 87.749 \\ 
        //ELG   &   547 & 1,864 &   0 &  0 &  0 &  0 & 2,411 & 1,864 & 1,864 & 77.310 & 77.310 \\ 
    //FakeQSO   &     1 &    88 &   0 &  0 &  0 &  0 &    90 &    88 &    88 & 98.389 & 98.389 \\ 
    //FakeLRG   &     2 &    47 &   0 &  0 &  0 &  0 &    50 &    47 &    47 & 94.493 & 94.493 \\ 
// Same without impr
    //QSOLy-a   &     0 &     1 &   3 & 10 & 21 & 11 &    49 &   185 &    49 & 98.396 & 74.100 \\ 
  //QSOTracer   &     1 &   118 &   0 &  0 &  0 &  0 &   119 &   118 &   118 & 98.412 & 98.412 \\ 
        //LRG   &    18 &    36 & 243 &  0 &  0 &  0 &   298 &   523 &   280 & 93.744 & 87.652 \\ 
        //ELG   &   612 & 1,798 &   0 &  0 &  0 &  0 & 2,411 & 1,798 & 1,798 & 74.584 & 74.584 \\ 
    //FakeQSO   &     1 &    88 &   0 &  0 &  0 &  0 &    90 &    88 &    88 & 98.397 & 98.397 \\ 
    //FakeLRG   &     2 &    47 &   0 &  0 &  0 &  0 &    50 &    47 &    47 & 94.464 & 94.464 \\ 
    //
    //With QSO then LRG then ELG
    ////    QSOLy-a   &     0 &     1 &   3 & 10 & 21 & 11 &    49 &   185 &    49 & 98.380 & 74.092 ÖÖ 
  //QSOTracer   &     1 &   118 &   0 &  0 &  0 &  0 &   119 &   118 &   118 & 98.401 & 98.401 ÖÖ 
        //LRG   &    18 &    35 & 245 &  0 &  0 &  0 &   298 &   525 &   280 & 93.899 & 88.018 ÖÖ 
        //ELG   &   622 & 1,788 &   0 &  0 &  0 &  0 & 2,411 & 1,788 & 1,788 & 74.184 & 74.184 ÖÖ 
    //FakeQSO   &     1 &    88 &   0 &  0 &  0 &  0 &    90 &    88 &    88 & 98.394 & 98.394 ÖÖ 
    //FakeLRG   &     2 &    47 &   0 &  0 &  0 &  0 &    50 &    47 &    47 & 94.590 & 94.590 ÖÖ 



// SS SF = 0, repartition
//# Results :
//Obs per sqd and percentages
           //0 &     1 &   2 &  3 &  4 &  5 &     6 &     7 &     8 &      9 &     10 \\ 
    //QSOLy-a   &     0 &     1 &   5 & 12 & 19 & 10 &    49 &   179 &    49 & 99.125 & 71.879 \\ 
  //QSOTracer   &     1 &   113 &   0 &  0 &  0 &  0 &   114 &   113 &   113 & 99.123 & 94.538 \\ 
        //LRG   &    15 &    42 & 241 &  0 &  0 &  0 &   298 &   524 &   283 & 94.934 & 87.842 \\ 
        //ELG   &   479 & 1,931 &   0 &  0 &  0 &  0 & 2,411 & 1,931 & 1,931 & 80.109 & 80.109 \\ 
    //FakeQSO   &     0 &    85 &   0 &  0 &  0 &  0 &    85 &    85 &    85 & 99.117 & 94.556 \\ 
    //FakeLRG   &     2 &    46 &   0 &  0 &  0 &  0 &    48 &    46 &    46 & 95.212 & 92.536 \\ 
         //SS   &   140 &     0 &   0 &  0 &  0 &  0 &   140 &     0 &     0 &      0 &      0 \\ 
         //SF   & 1,400 &     0 &   0 &  0 &  0 &  0 & 1,400 &     0 &     0 &      0 &      0 \\ 

  //45,831,177 assignments in total (85.9388 % of all fibers)

