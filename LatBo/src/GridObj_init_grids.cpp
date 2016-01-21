/* This file contains the routines necessary to initialise the macroscopic quantities and the grids and labels.
*/

#include "../inc/stdafx.h"
#include "../inc/GridObj.h"
#include "../inc/MpiManager.h"
#include "../inc/definitions.h"
#include "../inc/globalvars.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <math.h>

using namespace std;

// ***************************************************************************************************
void GridObj::LBM_init_getInletProfile() {

	unsigned int i, j;
	double y, tmp;
	std::vector<double> ybuffer, uxbuffer, uybuffer, uzbuffer;	

	// Buffer information from file
	std::ifstream inletfile;
	inletfile.open("./output/inlet_profile.in", std::ios::in);
	if (!inletfile.is_open()) {
		// Error opening file
		std::cout << "Error: See Log File" << std::endl;
		*GridUtils::logfile << "Cannot open inlet profile file named \"inlet_profile.in\". Exiting." << std::endl;
		exit(EXIT_FAILURE);

	} else {

		std::string line_in;	// String to store line
		std::istringstream iss;	// Buffer stream

		while( !inletfile.eof() ) {

			// Get line and put in buffer
			std::getline(inletfile,line_in,'\n');
			iss.str(line_in);
			iss.seekg(0); // Reset buffer position to start of buffer

			// Get y position
			iss >> tmp;
			ybuffer.push_back(tmp);

			// Get x velocity
			iss >> tmp;
			uxbuffer.push_back(tmp);

			// Get y velocity
			iss >> tmp;
			uybuffer.push_back(tmp);

			// Get z velocity
			iss >> tmp;
			uzbuffer.push_back(tmp);

		}

	}


	// Resize vectors
	ux_in.resize(YPos.size());
	uy_in.resize(YPos.size());
	uz_in.resize(YPos.size());

	// Loop over site positions (for left hand inlet, y positions)
	for (j = 0; j < YPos.size(); j++) {

		// Get Y-position
		y = YPos[j];

		// Find values either side of desired
		for (i = 0; i < ybuffer.size(); i++) {

			// Bottom point
			if (i == 0 && ybuffer[i] > y) {
				
				// Extrapolation
				ux_in[j] = -(uxbuffer[i+1] - uxbuffer[i]) + uxbuffer[i];
				uy_in[j] = -(uybuffer[i+1] - uybuffer[i]) + uybuffer[i];
				uz_in[j] = -(uzbuffer[i+1] - uzbuffer[i]) + uzbuffer[i];
				break;

			// Top point
			} else if (i == ybuffer.size()-1 && ybuffer[i] < y) {

				// Extrapolation
				ux_in[j] = (uxbuffer[i] - uxbuffer[i-1]) + uxbuffer[i];
				uy_in[j] = (uybuffer[i] - uybuffer[i-1]) + uybuffer[i];
				uz_in[j] = (uzbuffer[i] - uzbuffer[i-1]) + uzbuffer[i];
				break;


			// Any middle point
			} else if (ybuffer[i] < y && ybuffer[i+1] > y) {

				// Interpolation
				ux_in[j] = ((uxbuffer[i+1] - uxbuffer[i])/(ybuffer[i+1] - ybuffer[i])) * (y-ybuffer[i]) + uxbuffer[i];
				uy_in[j] = ((uybuffer[i+1] - uybuffer[i])/(ybuffer[i+1] - ybuffer[i])) * (y-ybuffer[i]) + uybuffer[i];
				uz_in[j] = ((uzbuffer[i+1] - uzbuffer[i])/(ybuffer[i+1] - ybuffer[i])) * (y-ybuffer[i]) + uzbuffer[i];
				break;

			} else if (ybuffer[i] == y ) {

				// Copy
				ux_in[j] = uxbuffer[i];
				uy_in[j] = uybuffer[i];
				uz_in[j] = uzbuffer[i];
				break;			
			
			} else {

				continue;

			}
				
				
		}

	}


}

// ***************************************************************************************************

// Initialise velocity method
void GridObj::LBM_init_vel ( ) {

#ifdef UNIFORM_INLET
	// Max velocity
	double u_in[3] = {u_0x, u_0y, u_0z};
#endif
	
	// Get grid sizes
	int N_lim = XPos.size();
	int M_lim = YPos.size();
	int K_lim = ZPos.size();


	// Loop over grid
	for (int i = 0; i < N_lim; i++) {
		for (int j = 0; j < M_lim; j++) {
			for (int k = 0; k < K_lim; k++) {
				
				
				for (size_t d = 0; d < dims; d++) {

#ifdef NO_FLOW
					// No flow case
					u(i,j,k,d,M_lim,K_lim,dims) = 0.0;

#elif defined UNIFORM_INLET

					// Uniform initialise
					u(i,j,k,d,M_lim,K_lim,dims) = u_in[d];

#endif
				}


#if (!defined NO_FLOW && !defined UNIFORM_INLET)

				/* Input velocity is specified by individual vectors for x, y and z which 
				 * have either been read in from an input file or defined by an expression 
				 * given in the definitions.
				 */
				u(i,j,k,0,M_lim,K_lim,dims) = u_0x;
				u(i,j,k,1,M_lim,K_lim,dims) = u_0y;
#if (dims == 3)
				u(i,j,k,2,M_lim,K_lim,dims) = u_0z;
#endif


#endif



			}
		}
	}

#if defined SOLID_BLOCK_ON || defined WALLS_ON || defined WALLS_ON_2D
	// Perform solid site reset of velocity
	bc_solid_site_reset();
#endif

}


// ***************************************************************************************************

// Initialise density method
void GridObj::LBM_init_rho ( ) {

	// Get grid sizes
	int N_lim = XPos.size();
	int M_lim = YPos.size();
	int K_lim = ZPos.size();

	// Loop over grid
	for (int i = 0; i < N_lim; i++) {
		for (int j = 0; j < M_lim; j++) {		
			for (int k = 0; k < K_lim; k++) {

				// Uniform Density
				rho(i,j,k,M_lim,K_lim) = rho_in;
			}
		}
	}

}

// ***************************************************************************************************

// Non-MPI initialise level 0 grid wrapper
void GridObj::LBM_init_grid( ) {

	// Set default value for the following MPI-specific settings
	std::vector<unsigned int> local_size;
	std::vector< std::vector<unsigned int> > global_edge_ind;
	std::vector< std::vector<double> > global_edge_pos;

	// Set local size to total grid size
	local_size.push_back(N);
	local_size.push_back(M);
	local_size.push_back(K);

	// Set global edge indices and position indices arbitrarily to zero as they won't be accessed anyway
	global_edge_ind.resize(1, std::vector<unsigned int>(1) );
	global_edge_ind[0][0] = 0;	
	global_edge_pos.resize(1, std::vector<double>(1) );
	global_edge_pos[0][0] = 0.0;

	// Call MPI initialiser wiht these default options
	LBM_init_grid(local_size, global_edge_ind, global_edge_pos);

}


// ***************************************************************************************************

// Initialise level 0 grid method
void GridObj::LBM_init_grid( std::vector<unsigned int> local_size, 
							std::vector< std::vector<unsigned int> > global_edge_ind, 
							std::vector< std::vector<double> > global_edge_pos ) {
	
	// Store physical spacing
	// Global dimensions
	double Lx = b_x - a_x;
	double Ly = b_y - a_y;
	double Lz = b_z - a_z;
	dx = 2*(Lx/(2*N));
	dy = 2*(Ly/(2*M));
	dz = 2*(Lz/(2*K));

	// Physical time step = physical grid spacing
	dt = dx;
	


	////////////////////////////
	// Check input parameters //
	////////////////////////////

#if (dims == 3)
	// Check that lattice volumes are cubes in 3D
	if ( (Lx/N) != (Ly/M) || (Lx/N) != (Lz/K) ) {
		std::cout << "Error: See Log File" << std::endl;
		*GridUtils::logfile << "Need to have lattice volumes which are cubes -- either change N/M/K or change domain dimensions. Exiting." << std::endl;
		exit(EXIT_FAILURE);
	}
	
#else
	// 2D so need square lattice cells
	if ( (Lx/N) != (Ly/M) ) {
		std::cout << "Error: See Log File" << std::endl;
		*GridUtils::logfile << "Need to have lattice cells which are squares -- either change N/M or change domain dimensions. Exiting." << std::endl;
		exit(EXIT_FAILURE);
	}

#endif

    // Checks to make sure grid size is suitable for refinement
	if (NumLev != 0) {

#if (dims == 3)
		for (int reg = 0; reg < NumReg; reg++) {
			// Check grid is big enough to allow embedded refinement of factor 2
			if (	(
					RefXend[level][reg]-RefXstart[level][reg]+1 < 3 || 
					RefYend[level][reg]-RefYstart[level][reg]+1 < 3 || 
					RefZend[level][reg]-RefZstart[level][reg]+1 < 3
					) || (
					(
					RefXend[level][reg]-RefXstart[level][reg]+1 == 3 ||
					RefYend[level][reg]-RefYstart[level][reg]+1 == 3 ||
					RefZend[level][reg]-RefZstart[level][reg]+1 == 3
					) && NumLev > 1 )
				) {
					std::cout << "Error: See Log File" << std::endl;
					*GridUtils::logfile << "Refined region is too small to support refinement. Exiting." << std::endl;
					exit(EXIT_FAILURE);
			}
		}
#else
		for (int reg = 0; reg < NumReg; reg++) {
			// Check grid is big enough to allow embedded refinement of factor 2
			if (	(
					RefXend[level][reg]-RefXstart[level][reg]+1 < 3 || 
					RefYend[level][reg]-RefYstart[level][reg]+1 < 3
					) || (
					(
					RefXend[level][reg]-RefXstart[level][reg]+1 == 3 ||
					RefYend[level][reg]-RefYstart[level][reg]+1 == 3
					) && NumLev > 1 )
				) {
					std::cout << "Error: See Log File" << std::endl;
					*GridUtils::logfile << "Refined region is too small to support refinement. Exiting." << std::endl;
					exit(EXIT_FAILURE);
			}
		}
#endif
	}


    ///////////////////
	// Checks passed //
	///////////////////

	// Get local grid sizes (includes overlap)
	size_t N_lim = local_size[0];
	size_t M_lim = local_size[1];
#if (dims == 3)
	size_t K_lim = local_size[2];
#else
	size_t K_lim = 1;
#endif
	

	// NODE NUMBERS on L0
	/* When using MPI:
	 * Node numbers should be specified in the global system.
	 * The overlapping sites have the same index as an index
	 * on its overlapping grid. The setup assumes periodicity
	 * on all edges, even where the edge of the domain is a 
	 * boundary and so halo cells exist on all edges.
	 */
#ifdef BUILD_FOR_MPI	

	// Build index vectors
	XInd = GridUtils::onespace( (int)global_edge_ind[0][MpiManager::my_rank], (int)global_edge_ind[1][MpiManager::my_rank] - 1 );
	YInd = GridUtils::onespace( (int)global_edge_ind[2][MpiManager::my_rank], (int)global_edge_ind[3][MpiManager::my_rank] - 1 );
	ZInd = GridUtils::onespace( (int)global_edge_ind[4][MpiManager::my_rank], (int)global_edge_ind[5][MpiManager::my_rank] - 1 );

	// Add overlap indices to both ends of the vector taking into account periodicity
	XInd.insert( XInd.begin(), (XInd[0]-1 + N) % N ); XInd.insert( XInd.end(), (XInd[XInd.size()-1]+1 + N) % N );
	YInd.insert( YInd.begin(), (YInd[0]-1 + M) % M ); YInd.insert( YInd.end(), (YInd[YInd.size()-1]+1 + M) % M );
#if (dims == 3)
	ZInd.insert( ZInd.begin(), (ZInd[0]-1 + K) % K ); ZInd.insert( ZInd.end(), (ZInd[ZInd.size()-1]+1 + K) % K );
#endif

#else
	// When not builiding for MPI indices are straightforward
	XInd = GridUtils::onespace( 0, N-1 );
	YInd = GridUtils::onespace( 0, M-1 );
	ZInd = GridUtils::onespace( 0, K-1 );
#endif



	// L0 lattice site POSITION VECTORS
	/* When using MPI:
	 * As with the indices, the overlap is assume periodic
	 * in all directions.
	 */

#ifdef BUILD_FOR_MPI

	// Create position vectors excluding overlap
	XPos = GridUtils::linspace( global_edge_pos[0][MpiManager::my_rank] + dx/2, global_edge_pos[1][MpiManager::my_rank] - dx/2, N_lim-2 );
	YPos = GridUtils::linspace( global_edge_pos[2][MpiManager::my_rank] + dy/2, global_edge_pos[3][MpiManager::my_rank] - dy/2, M_lim-2 );
	ZPos = GridUtils::linspace( global_edge_pos[4][MpiManager::my_rank] + dz/2, global_edge_pos[5][MpiManager::my_rank] - dz/2, K_lim-2 );

	// Add overlap sites taking into account periodicity
	XPos.insert( XPos.begin(), fmod(XPos[0]-dx + Lx, Lx) ); XPos.insert( XPos.end(), fmod(XPos[XPos.size()-1]+dx + Lx, Lx) );
	YPos.insert( YPos.begin(), fmod(YPos[0]-dy + Ly, Ly) ); YPos.insert( YPos.end(), fmod(YPos[YPos.size()-1]+dy + Ly, Ly) );
#if (dims == 3)
	ZPos.insert( ZPos.begin(), fmod(ZPos[0]-dz + Lz, Ly) ); ZPos.insert( ZPos.end(), fmod(ZPos[ZPos.size()-1]+dz + Lz, Lz) );
#endif

	// Update the sender/recv layer positions in the MpiManager
	MpiManager* mpim = MpiManager::getInstance();

	// X
	mpim->sender_layer_pos.X[0] = XPos[1] - dx/2; mpim->sender_layer_pos.X[1] = XPos[1] + dx/2;
	mpim->sender_layer_pos.X[2] = XPos[local_size[0] - 2] - dx/2; mpim->sender_layer_pos.X[3] = XPos[local_size[0] - 2] + dx/2;
	mpim->recv_layer_pos.X[0] = XPos[0] - dx/2; mpim->recv_layer_pos.X[1] = XPos[0] + dx/2;
	mpim->recv_layer_pos.X[2] = XPos[local_size[0] - 1] - dx/2; mpim->recv_layer_pos.X[3] = XPos[local_size[0] - 1] + dx/2;
	// Y
	mpim->sender_layer_pos.Y[0] = YPos[1] - dy/2; mpim->sender_layer_pos.Y[1] = YPos[1] + dy/2;
	mpim->sender_layer_pos.Y[2] = YPos[local_size[1] - 2] - dy/2; mpim->sender_layer_pos.Y[3] = YPos[local_size[1] - 2] + dy/2;
	mpim->recv_layer_pos.Y[0] = YPos[0] - dy/2; mpim->recv_layer_pos.Y[1] = YPos[0] + dy/2;
	mpim->recv_layer_pos.Y[2] = YPos[local_size[0] - 1] - dy/2; mpim->recv_layer_pos.Y[3] = YPos[local_size[0] - 1] + dy/2;
	// Z
#if (dims == 3)
	mpim->sender_layer_pos.Z[0] = ZPos[1] - dz/2; mpim->sender_layer_pos.Z[1] = ZPos[1] + dz/2;
	mpim->sender_layer_pos.Z[2] = ZPos[local_size[2] - 2] - dz/2; mpim->sender_layer_pos.Z[3] = ZPos[local_size[2] - 2] + dz/2;
	mpim->recv_layer_pos.Z[0] = ZPos[0] - dz/2; mpim->recv_layer_pos.Z[1] = ZPos[0] + dz/2;
	mpim->recv_layer_pos.Z[2] = ZPos[local_size[0] - 1] - dz/2; mpim->recv_layer_pos.Z[3] = ZPos[local_size[0] - 1] + dz/2;
#endif


#else
	// When not builiding for MPI positions are straightforward
	XPos = GridUtils::linspace( a_x + dx/2, b_x - dx/2, N );
	YPos = GridUtils::linspace( a_y + dy/2, b_y - dy/2, M );
	ZPos = GridUtils::linspace( a_z + dz/2, b_z - dz/2, K );
#endif

	

	// Define TYPING MATRICES
	LatTyp.resize( N_lim*M_lim*K_lim );

	// Typing defined as follows:
	/*
	0 == boundary site
	1 == coarse site
	2 == fine/refined site
	3 == TL to upper (coarser) level
	4 == TL to lower (finer) level
	5 == Undefined
	6 == Undefined
	7 == Inlet
	8 == Outlet
	*/

	// Label as coarse site
	std::fill(LatTyp.begin(), LatTyp.end(), 1);

	// Add boundary-specific labels
	LBM_init_bound_lab();



	// Initialise L0 MACROSCOPIC quantities

	// Get the inlet profile data
#ifdef USE_INLET_PROFILE

	for (int n = 0; n < max_ranks; n++) {

		if (n == my_rank) {

			LBM_init_getInletProfile();
		}
	}
#endif

	// Velocity field
	u.resize( N_lim*M_lim*K_lim*dims );
	LBM_init_vel();
	
	// Density field
	rho.resize( N_lim*M_lim*K_lim );
	LBM_init_rho();

	// Cartesian force vector
	force_xyz.resize(N_lim*M_lim*K_lim*dims, 0.0);

	// Lattice force vector
	force_i.resize(N_lim*M_lim*K_lim*nVels, 0.0);

	// Time averaged quantities
	rho_timeav.resize(N_lim*M_lim*K_lim, 0.0);
	ui_timeav.resize(N_lim*M_lim*K_lim*dims, 0.0);
	uiuj_timeav.resize(N_lim*M_lim*K_lim*(3*dims-3), 0.0);


	// Initialise L0 POPULATION matrices (f, feq)
	f.resize( N_lim*M_lim*K_lim*nVels );
	feq.resize( N_lim*M_lim*K_lim*nVels );



	// Loop over grid
	for (size_t i = 0; i < N_lim; i++) {
		for (size_t j = 0; j < M_lim; j++) {
			for (size_t k = 0; k < K_lim; k++) {
				for (size_t v = 0; v < nVels; v++) {

					// Initialise f to feq
					f(i,j,k,v,M_lim,K_lim,nVels) = LBM_collide( i, j, k, v );

				}
			}
		}
	}
	feq = f; // Make feq = feq too


	// Initialise OTHER parameters
	// Compute kinematic viscosity based on target Reynolds number
#if defined IBM_ON && defined INSERT_CIRCLE_SPHERE
	// If IBM circle use diameter (in lattice units i.e. rescale wrt to physical spacing)
	nu = (ibb_r*2 / dx) * u_ref / Re;
#elif defined IBM_ON && defined INSERT_RECTANGLE_CUBOID
	// If IBM rectangle use y-dimension (in lattice units)
	nu = (ibb_l / dx) * u_ref / Re;
#elif defined SOLID_BLOCK_ON
	// Use object height
	nu = (obj_y_max - obj_y_min) * u_ref / Re;
#else
	// If no object then use domain height (in lattice units)
	nu = M * u_ref / Re;
#endif

	// Relaxation frequency on L0
	// Assign relaxation frequency using lattice viscosity
	omega = 1 / ( (nu / pow(cs,2) ) + .5 );

	/* Above is valid for L0 only when dx = 1 -- general expression is:
	 * omega = 1 / ( ( (nu * dt) / (pow(cs,2)*pow(dx,2)) ) + .5 );
	 */

#ifdef USE_MRT
	double tmp[] = mrt_relax;
	for (int i = 0; i < nVels; i++) {
		mrt_omega.push_back(tmp[i]);
	}
#endif

}
	


// ***************************************************************************************************

// Method to initialise the quantities for a refined subgrid assuming a volumetric configuration. Parent grid is passed by reference
void GridObj::LBM_init_subgrid (GridObj& pGrid) {

	// Declarations
	unsigned int IndXstart, IndYstart, IndZstart = 0;

	/* MPI specific setup:
	 * 1. Store coarse grid refinement limits;
	 * 2. Get node numbering ends in global system
	 *
	 * These are stored as local indices as they are used to map between the 
	 * fine and coarse grid cells during multi-grid operations. Therefore, we 
	 * must only store local values relevant to the grid on the rank and not the
	 * refined region as a whole or mapping will not be correct.
	 *
	 * When not using MPI, these can be read straight from the definitions file and 
	 * converted to local coordinates corresponding to the parent grid.
	 * However, when using MPI, the edges of the refined grid might not be on this 
	 * rank so we must round the coarse limits to the edge of the parent grid so 
	 * the correct offset is supplied to the mapping routine.
	 */

	// If region is not contained on a single rank adjust limits accordingly:

	// X
	if ( (int)RefXstart[pGrid.level][region_number] < pGrid.XInd[1] - 1 ) {
		// Set grid limits to start edge of the rank
		CoarseLimsX[0] = 0;
		// Compute actual node start
		IndXstart = ((pGrid.XInd[CoarseLimsX[0]] - RefXstart[pGrid.level][region_number]) * 2);
	} else {
		// Get limit from parent as edge on this rank (takes into account overlap if present)
		CoarseLimsX[0] = RefXstart[pGrid.level][region_number] - pGrid.XInd[1] + 1;
		// Start index is simply zero
		IndXstart = 0;
	}
	if ( (int)RefXend[pGrid.level][region_number] > pGrid.XInd[pGrid.XInd.size() - 2] + 1 ) {
		// Set grid limits to end edge of the rank
		CoarseLimsX[1] = pGrid.XInd.size() - 1;
	} else {
		CoarseLimsX[1] = RefXend[pGrid.level][region_number] - pGrid.XInd[1] + 1;
	}

	// Y
	if ( (int)RefYstart[pGrid.level][region_number] < pGrid.YInd[1] - 1 ) {
		CoarseLimsY[0] = 0;
		IndYstart = ((pGrid.YInd[CoarseLimsY[0]] - RefYstart[pGrid.level][region_number]) * 2);
	} else {
		CoarseLimsY[0] = RefYstart[pGrid.level][region_number] - pGrid.YInd[1] + 1;
		IndYstart = 0;
	}
	if ( (int)RefYend[pGrid.level][region_number] > pGrid.YInd[pGrid.YInd.size() - 2] + 1 ) {
		CoarseLimsY[1] = pGrid.YInd.size() - 1;
	} else {
		CoarseLimsY[1] = RefYend[pGrid.level][region_number] - pGrid.YInd[1] + 1;
	}

#if (dims == 3)
	// Z
	if ( (int)RefZstart[pGrid.level][region_number] < pGrid.ZInd[1] - 1 ) {
		CoarseLimsZ[0] = 0;
		IndZstart = ((pGrid.ZInd[CoarseLimsZ[0]] - RefZstart[pGrid.level][region_number]) * 2);
	} else {
		CoarseLimsZ[0] = RefZstart[pGrid.level][region_number] - pGrid.ZInd[1] + 1;
		IndZstart = 0;
	}
	if ( (int)RefZend[pGrid.level][region_number] > pGrid.ZInd[pGrid.ZInd.size() - 2] + 1 ) {
		CoarseLimsZ[1] = pGrid.ZInd.size() - 1;
	} else {
		CoarseLimsZ[1] = RefZend[pGrid.level][region_number] - pGrid.ZInd[1] + 1;
	}
#else
	// Reset the refined region z-limits if only 2D
	for (int i = 0; i < 2; i++) {
		CoarseLimsZ[i] = 0;
	}
#endif

	/* Note that the CoarseLims are local values used to identify how much and which part of the 
	 * parent grid is covered by the child sub-grid and might not conincide with edges of global 
	 * refined patch defined by the input file if broken up across ranks. So we get the offset 
	 * in position from the local refined patch edges specific to this rank. */

	// Get positons of local refined patch edges
	double posOffsetX[2] = { pGrid.XPos[ CoarseLimsX[0] ], pGrid.XPos[ CoarseLimsX[1] ] };
	double posOffsetY[2] = { pGrid.YPos[ CoarseLimsY[0] ], pGrid.YPos[ CoarseLimsY[1] ] };
#if (dims == 3)
	double posOffsetZ[2] = { pGrid.ZPos[ CoarseLimsZ[0] ], pGrid.ZPos[ CoarseLimsZ[0] ] };
#else
	double posOffsetZ[2] = { 0.0, 0.0 };
#endif

	// Get local grid size of the sub grid based on limits
	int local_size[dims] = {
		(int)((CoarseLimsX[1] - CoarseLimsX[0] + .5)*2) + 1,
		(int)((CoarseLimsY[1] - CoarseLimsY[0] + .5)*2) + 1
#if (dims == 3)
		, (int)((CoarseLimsZ[1] - CoarseLimsZ[0] + .5)*2) + 1
#endif
	};



	// Generate NODE NUMBERS
	XInd = GridUtils::onespace( IndXstart, IndXstart + local_size[0] - 1 );
	YInd = GridUtils::onespace( IndYstart, IndYstart + local_size[1] - 1 );
#if (dims == 3)
	ZInd = GridUtils::onespace( IndZstart, IndZstart + local_size[2] - 1 );
#else
	ZInd.insert(ZInd.begin(), 0); // Default for 2D
#endif



	// Generate POSITION VECTORS of nodes
	// Define spacing
	dx = pGrid.dx/2;
	dy = dx;
	dz = dx;

	// Populate the position vectors
	XPos = GridUtils::linspace(posOffsetX[0] - dx/2, (posOffsetX[0] - dx/2) + (XInd.size() - 1) * dx, XInd.size() );
	YPos = GridUtils::linspace(posOffsetY[0] - dy/2, (posOffsetY[0] - dy/2) + (YInd.size() - 1) * dy, YInd.size() );
#if dims == 3
	ZPos = GridUtils::linspace(posOffsetZ[0] - dz/2, (posOffsetZ[0] - dz/2) + (ZInd.size() - 1) * dz, ZInd.size() );
#else
	ZPos.insert( ZPos.begin(), 1 ); // 2D default
#endif


	// Generate TYPING MATRICES

	// Typing defined as follows:
	/*
	0 == boundary site
	1 == coarse site
	2 == fine/refined site
	3 == TL to upper (coarser) level
	4 == TL to lower (finer) level
	5 == Undefined
	6 == Undefined
	7 == Inlet
	8 == Outlet
	*/

	// Get local grid sizes
	size_t N_lim = XInd.size();
	size_t M_lim = YInd.size();
	size_t K_lim = ZInd.size();

	// Resize
	LatTyp.resize( YInd.size() * XInd.size() * ZInd.size() );

	// Default labelling of coarse
	std::fill(LatTyp.begin(), LatTyp.end(), 1);

	// Call refined labelling routine passing parent grid
	LBM_init_refined_lab(pGrid);

	

	// Assign MACROSCOPIC quantities
	// Velocity
	u.resize(N_lim * M_lim * K_lim * dims);
	LBM_init_vel( );

	// Density
	rho.resize(N_lim * M_lim * K_lim);
	LBM_init_rho( );

	// Cartesian force vector
	force_xyz.resize(N_lim * M_lim * K_lim * dims, 0.0);

	// Lattice force vector
	force_i.resize(N_lim * M_lim * K_lim * nVels, 0.0);

	// Time averaged quantities
	rho_timeav.resize(N_lim*M_lim*K_lim, 0.0);
	ui_timeav.resize(N_lim*M_lim*K_lim*dims, 0.0);
	uiuj_timeav.resize(N_lim*M_lim*K_lim*(3*dims-3), 0.0);


	// Generate POPULATION MATRICES for lower levels
	// Resize
	f.resize(N_lim * M_lim * K_lim * nVels);
	feq.resize(N_lim * M_lim * K_lim * nVels);
	

	// Loop over grid
	for (size_t i = 0; i != N_lim; ++i) {
		for (size_t j = 0; j != M_lim; ++j) {
			for (size_t k = 0; k != K_lim; ++k) {
				for (int v = 0; v < nVels; v++) {
					
					// Initialise f to feq
					f(i,j,k,v,M_lim,K_lim,nVels) = LBM_collide( i, j, k, v );

				}
			}
		}
	}
	feq = f; // Set feq to feq

	// Compute relaxation time from coarser level assume refinement by factor of 2
	omega = 1 / ( ( (1/pGrid.omega - .5) *2) + .5);

#ifdef USE_MRT
	
	// MRT relaxation times on the finer grid related to coarse in same way as SRT
	for (size_t i = 0; i < nVels; i++) {
		mrt_omega.push_back( 1 / ( ( (1/pGrid.mrt_omega[i] - .5) *2) + .5) );
	}

#endif

}

// ***************************************************************************************************
void GridObj::LBM_init_solid_lab() {

#ifdef SOLID_BLOCK_ON
	// Return if not to be put on the current grid
	if (block_on_grid_lev != level || block_on_grid_reg != region_number) return;

	// Get local grid sizes
	int N_lim = XPos.size();
	int M_lim = YPos.size();
	int K_lim = ZPos.size();

	// Declarations
	int i, j, k, local_i, local_j, local_k;

	/* Check solid block contained on the global grid specified in the definitions file.
	 * If not then exit as user has specifed block outside the grid on which they want it 
	 * placed. */

	// Get global grid sizes
	int Ng_lim = (RefXend[level-1][region_number] - RefXstart[level-1][region_number] + 1) * 2;
	int Mg_lim = (RefYend[level-1][region_number] - RefYstart[level-1][region_number] + 1) * 2;
	int Kg_lim = (RefZend[level-1][region_number] - RefZstart[level-1][region_number] + 1) * 2;

	// Check block placement -- must not be on TL (last two sites) if on a level other than 0
	if	(
		(
		
		(level == 0) && 
		
		(obj_x_max > Ng_lim - 1 || obj_x_min < 0 || obj_y_max > Mg_lim - 1 || obj_y_min < 0 
#if (dims == 3)
		|| obj_z_max > Kg_lim - 1 || obj_z_min < 0
#endif
		)

		) || (

		(level != 0) && 

		(obj_x_max >= Ng_lim - 2 || obj_x_min <= 1 || obj_y_max >= Mg_lim - 2 || obj_y_min <= 1 
#if (dims == 3)
		|| obj_z_max >= Kg_lim - 2 || obj_z_min <= 1
#endif
		)
	
		)
		){

		// Block outside grid
		std::cout << "Error: See Log File" << std::endl;
		*GridUtils::logfile << "Block is placed outside or on the TL of the selected grid. Exiting." << std::endl;
		exit(EXIT_FAILURE);
	}


	// Loop over object definition in global indices
	for (i = obj_x_min; i <= obj_x_max; i++) {
		for (j = obj_y_min; j <= obj_y_max; j++) {
			for (k = obj_z_min; k <= obj_z_max; k++) {

				// Only label if the site is on current rank
				if ( GridUtils::isOnThisRank(i,j,k,*this) ) {
						
						// Map global indices to local indices
						local_i = i - XInd[1] + 1;
						local_j = j - YInd[1] + 1;
#if (dims == 3)
						local_k = k - ZInd[1] + 1;
#else
						local_k = k;
#endif

						LatTyp(local_i,local_j,local_k,M_lim,K_lim) = 0;
				}

			}
		}
	}

#endif

}

// ***************************************************************************************************
// Initialise wall and object labels method (L0 only)
void GridObj::LBM_init_bound_lab ( ) {

	// Typing defined as follows:
	/*
	0 == boundary site
	1 == coarse site
	2 == fine/refined site
	3 == TL to upper (coarser) level
	4 == TL to lower (finer) level
	5 == Undefined
	6 == Undefined
	7 == Inlet
	8 == Outlet
	*/

	// Get grid sizes
	int N_lim = XPos.size();
	int M_lim = YPos.size();
	int K_lim = ZPos.size();

#if defined WALLS_ON || defined INLET_ON || defined OUTLET_ON || defined WALLS_ON_2D
	int i, j, k;
#endif

	// Try to add the solid block
	LBM_init_solid_lab();

#ifdef INLET_ON
	// Left hand face only

	// Check for potential singularity in BC
	if (u_max == 1 || u_ref == 1) {
		// Singularity so exit
		std::cout << "Error: See Log File" << std::endl;
		*GridUtils::logfile << "Inlet BC fails with u_0x = 1, choose something else. Exiting." << std::endl;
		exit(EXIT_FAILURE);
	}

	// Search index vector to see if left hand wall on this rank
	for (i = 0; i < N_lim; i++ ) {
		if (XInd[i] == 0) {		// Wall found

			// Label inlet
			for (j = 0; j < M_lim; j++) {
				for (k = 0; k < K_lim; k++) {

					// Inlet site
					LatTyp(i,j,k,M_lim,K_lim) = 7;

				}
			}
			break;

		}
	}
#endif

#ifdef OUTLET_ON
	// Right hand face only

	// Search index vector to see if right hand wall on this rank
	for (i = 0; i < N_lim; i++ ) {
		if (XInd[i] == N-1) {		// Wall found

			// Label outlet
			for (j = 0; j < M_lim; j++) {
				for (k = 0; k < K_lim; k++) {

					LatTyp(i,j,k,M_lim,K_lim) = 8;

				}
			}
			break;

		}
	}
#endif

#if defined WALLS_ON && !defined WALLS_ON_2D && (dims == 3)

	// Search index vector to see if FRONT wall on this rank
	for (k = 0; k < K_lim; k++ ) {
		if (ZInd[k] == 0) {		// Wall found

			// Label wall
			for (i = 0; i < N_lim; i++) {
				for (j = 0; j < M_lim; j++) {

					LatTyp(i,j,k,M_lim,K_lim) = 0;

				}
			}
			break;

		}
	}

	// Search index vector to see if BACK wall on this rank
	for (k = 0; k < K_lim; k++ ) {
		if (ZInd[k] == K-1) {		// Wall found

			// Label wall
			for (i = 0; i < N_lim; i++) {
				for (j = 0; j < M_lim; j++) {

					LatTyp(i,j,k,M_lim,K_lim) = 0;

				}
			}
			break;

		}
	}

#endif

#if defined WALLS_ON
	// Search index vector to see if TOP wall on this rank
	for (j = 0; j < M_lim; j++ ) {
		if (YInd[j] == 0) {		// Wall found

			// Label wall
			for (i = 0; i < N_lim; i++) {
				for (k = 0; k < K_lim; k++) {

					LatTyp(i,j,k,M_lim,K_lim) = 0;

				}
			}
			break;

		}
	}

	// Search index vector to see if BOTTOM wall on this rank
	for (j = 0; j < M_lim; j++ ) {
		if (YInd[j] == M-1) {		// Wall found

			// Label wall
			for (i = 0; i < N_lim; i++) {
				for (k = 0; k < K_lim; k++) {

					LatTyp(i,j,k,M_lim,K_lim) = 0;

				}
			}
			break;

		}
	}
#endif

}

// ***************************************************************************************************

// Initialise refined labels method
void GridObj::LBM_init_refined_lab (GridObj& pGrid) {

	// Typing defined as follows:
	/*
	0 == boundary site
	1 == coarse site
	2 == fine/refined site
	3 == TL to upper (coarser) level
	4 == TL to lower (finer) level
	5 == Undefined
	6 == Undefined
	7 == Inlet
	8 == Outlet
	*/

	// Get parent local grid sizes
	size_t Np_lim = pGrid.XPos.size();
	size_t Mp_lim = pGrid.YPos.size();
	size_t Kp_lim = pGrid.ZPos.size();

	// Declare indices global and local
	int i, j, k, local_i, local_j, local_k;

	// Loop over global indices of refined patch and add "TL to lower" labels
    for (i = (int)RefXstart[pGrid.level][region_number]; i <= (int)RefXend[pGrid.level][region_number]; i++) {
        for (j = (int)RefYstart[pGrid.level][region_number]; j <= (int)RefYend[pGrid.level][region_number]; j++) {
#if (dims == 3)
			for (k = (int)RefZstart[pGrid.level][region_number]; k <= (int)RefZend[pGrid.level][region_number]; k++)
#else
			k = 0;
#endif
			{

#ifdef BUILD_FOR_MPI						
			// Compute local indices to correct LatTyp array on parent
			local_i = i - pGrid.XInd[1] + 1;
			local_j = j - pGrid.YInd[1] + 1;
			local_k = k - pGrid.ZInd[1] + 1;
#if (dims != 3)
			local_k = k;
#endif
#else
			local_i = i;
			local_j = j;
			local_k = k;						
#endif
				
			// Only act if the site is on parent rank (inc overlap) to avoid out of bounds errors
			if ( GridUtils::isOnThisRank(i,j,k,pGrid) ) {

				// If on the edge of the global refined patch then it is TL so label
				if	(
					(i == RefXstart[pGrid.level][region_number] || i == RefXend[pGrid.level][region_number]) ||
					(j == RefYstart[pGrid.level][region_number] || j == RefYend[pGrid.level][region_number])
#if (dims == 3)
					|| (k == RefZstart[pGrid.level][region_number] || k == RefZend[pGrid.level][region_number])
#endif
					) {

					// If parent site is fluid site then correct label
					if (pGrid.LatTyp(local_i,local_j,local_k,Mp_lim,Kp_lim) == 1) {
						// Change to "TL to lower" label
						pGrid.LatTyp(local_i,local_j,local_k,Mp_lim,Kp_lim) = 4;
					}

				} else {
					// Else, label it a "refined" site as in the middle of the global refined patch
					pGrid.LatTyp(local_i,local_j,local_k,Mp_lim,Kp_lim) = 2;

				}

			}

			}            
        }
    }
    
	// Generate grid type matrices for this level //

	// Get local grid sizes (includes overlap)
	size_t N_lim = XPos.size();
	size_t M_lim = YPos.size();
	size_t K_lim = ZPos.size();

	// Declare array for parent site indices
	std::vector<int> p;
	int par_label;
    
    // Loop over sub-grid and add labels based on parent site labels
    for (size_t i = 0; i < N_lim; i++) {
		for (size_t j = 0; j < M_lim; j++) {
#if (dims == 3)
			for (size_t k = 0; k < K_lim; k++)
#else
			size_t k = 0;
#endif
			{
				// Get parent site indices as locals for the current sub-grid local site
				p = GridUtils::revindmapref(
					i, CoarseLimsX[0], 
					j, CoarseLimsY[0], 
					k, CoarseLimsZ[0]
				);

				// Get parent site label using local indices
				par_label = pGrid.LatTyp(p[0],p[1],p[2],Mp_lim,Kp_lim);

				// If parent is a "TL to lower" then add "TL to upper" label
				if (par_label == 4) {
					LatTyp(i,j,k,M_lim,K_lim) = 3;
                
					// Else if parent is a "refined" label then label as coarse
				} else if (par_label == 2) {
					LatTyp(i,j,k,M_lim,K_lim) = 1;
                
					// Else parent label is some other kind of boundary so copy the
					// label to retain the behaviour onto this grid
				} else {
					LatTyp(i,j,k,M_lim,K_lim) = par_label;
                
					// If last site to be updated in fine block, change parent label
					// to ensure boundary values are pulled from fine grid
					if ((j % 2) != 0 && (i % 2) != 0) {
						pGrid.LatTyp(p[0],p[1],p[2],pGrid.YInd.size(),pGrid.ZInd.size()) = 4;
					}
				}
            }
        }
    }


	// Try to add the solid block labels
	LBM_init_solid_lab();
}

// ***************************************************************************************************