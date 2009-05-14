#include <util.h>
#include <hash.h>
#include <stdio.h>
#include <string.h>
#include <ecl_file.h>
#include <ecl_util.h>
#include <vector.h>
#include <ecl_grid.h>
#include <math.h>

#define WATER 1
#define GAS   2
#define OIL   4


/*****************************************************************/



void truncate_saturation(float * value) {
  util_apply_float_limits( value , 0.0 , 1.0);
}



bool has_phase( int phase_sum , int phase) {
  if ((phase_sum & phase) == 0)
    return false;
  else
    return true;
}


const float * safe_get_float_ptr( const ecl_kw_type * ecl_kw , const float * alternative) {
  if (ecl_kw != NULL)
    return ecl_kw_get_float_ptr(ecl_kw);
  else
    return alternative;
}




/*****************************************************************/

void print_usage() {
  printf("***************************************************************************\n");
  printf("  This program is used to calculate the change in graviational response \n");
  printf("  between two timesteps in an eclipse simulation.\n");
  printf("\n");
  printf(" USAGE: ./run_gravity.x [-h] base_name restart_time1 restart_time2 station_position_file\n");
  printf("\n");
  printf(" NOTE: The program needs a *.GRID, *.INIT, as well as the two restart files.\n");
  printf(" NOTE: The reservoir pore volume needs to be reported in the restart file: \n");
  printf(" NOTE:  Eclipse kw RPORV in RPTRST\n");
  
  printf("***************************************************************************\n");
  exit(1);
}


typedef struct {
  double utm_x; 
  double utm_y; 
  double depth; 
  double grav_diff;
} grav_station_type;


grav_station_type * grav_station_alloc(double x, double y, double d){
  grav_station_type * s = util_malloc(sizeof*s, __func__);
  s->utm_x = x;
  s->utm_y = y;
  s->depth = d;
  s->grav_diff = 0.0;
  return s;
}


void grav_diff_update(grav_station_type * g_s, double inc){
  g_s->grav_diff = g_s->grav_diff + inc;
}


void load_stations(vector_type * grav_stations , const char * filename) {
  FILE * stream = util_fopen(filename , "r");
  bool at_eof = false;
  while(!(at_eof)) {
    double x,y,d;
    int fscanf_return = fscanf(stream, "%lg%lg%lg", &x,&y,&d);
    if(fscanf_return ==3){
      grav_station_type * g = grav_station_alloc(x,y,d);
      vector_append_owned_ref(grav_stations, g, free);
    }
    //else if(fscanf_return == 0) {
    //  at_eof = true;
    //}
    else{
      at_eof = true;
      //util_abort("%s: something funky - only found %d numbers", __func__, fscanf_return);
    }
  }
  fclose(stream);
}
    





/**
   This function will load, and return two ecl_file_type instances with the
   restart information from the two relevant times. The input to this
   function is a (char **) pointer, taken directly from the argv input
   pointer.
   
   The function will start by calling ecl_util_get_file_type(input[0]), and
   depending on the return value from this call it will follow three
   different code-paths:


     ECL_FILE_OTHER: This means that the first argument should be
        interpreted not as an existing file name, but rather as an ECLIPSE
        base name. The program will look for restart info in files in the
        working directory with the following order:

	 1. Unified restart file - unformatted.
	 2. Non unified restart files - unformatted.
	 3. Unified restart file - formatted.
	 4. Non unified restart files - formatted.
	
	The search will stop at the first success, if no restart
	information is found the function will exit. The remaining
	arguments in input[] will not be considered, but observe that the
	use of ecl_base is signalled back to calling scope (through
	reference), and the calling scope will look for GRID file and INIT
	file also based on the ECLBASE found input[0]; formatted /
	unformatted will be as returned from the four-way switch above.
 
	Example:
	
	bash% run_gravity  ECLIPSE   10  128   xxxx
	


     ECL_RESTART_FILE: This means that input[0] is a non unified eclipse
        restart file, this file will be loaded. And it is ASSUMED that
        input[1] is the next non - unified restart file, loaded for the
        next report step.

	Example:

	bash% run_gravity ECLIPSE.X0010  ECLIPSE.X0128   xxx
	

     
     ECL_UNIFIED_RESTART_FILE: This means that input[1] and input[2] are
        interpreted as integers (i.e. report steps), and those two report
        steps will be loaded from the unified restart file pointed to by
        input[0].

	Example:

	bash% run_gravity ECLIPSE.UNRST  10 128 xxx

 

    Observe that in all the examples above 'xxx' signifies argv arguments
    which this function does not care about. On return the *arg_offset
    variable will be set to indicate this index:

    char ** input = argv[1];
    int     input_offset;
    ecl_restart_file_type ** restart_info = load_restart_info(input , &input_offset, ...);
    
    Then the next argument is: input[input_offset];
*/


static ecl_file_type ** load_restart_info(const char ** input,           /* Input taken directly from argv */
					  int           input_length,    /* The length of input. */
					  int         * arg_offset,      /* Integer - value corresponding to the *NEXT* element in input which should be used by the calling scope. */
					  bool        * use_eclbase,     /* Should input[0] be interpreted as an ECLBASE string? */
					  bool        * fmt_file) {      /* Only relevant if (*use_eclbase == true): was formatted file used? */
  
  
  ecl_file_type ** restart_files = util_malloc( 2 * sizeof * restart_files , __func__);
  int  report_nr;
  ecl_file_enum file_type;

  *use_eclbase = false;
  ecl_util_get_file_type( input[0] , &file_type , fmt_file , &report_nr );

  if (file_type == ECL_RESTART_FILE) {
    /* Loading from two non-unified restart files. */
    if (input_length >= 2) {
      ecl_util_get_file_type( input[1] , &file_type , fmt_file , &report_nr );
      if (file_type == ECL_RESTART_FILE) {
	restart_files[0] = ecl_file_fread_alloc( input[0] , true );
	restart_files[1] = ecl_file_fread_alloc( input[1] , true );
	*arg_offset = 2;
      } else print_usage();
    } else print_usage();
  } else if (file_type == ECL_UNIFIED_RESTART_FILE) {
    /* Loading from one unified restart file. */
    if (input_length >= 3) {
      int report1 , report2;
      if ((util_sscanf_int( input[1] , &report1) && util_sscanf_int( input[2] , &report2))) {
	restart_files[0] = ecl_file_fread_alloc_unrst_section( input[0] , report1 , true);
	restart_files[1] = ecl_file_fread_alloc_unrst_section( input[0] , report2 , true);
	*arg_offset = 3;
      } else
	print_usage();
    } else 
      print_usage();
  } else if (file_type == ECL_OTHER_FILE) {
    if (input_length >= 3) {
      int report1, report2;
      if (!(util_sscanf_int( input[1] , &report1) && util_sscanf_int( input[2] , &report2)))
	print_usage();
      else {
	/* 
	   input[0] is interpreted as an eclbase string, and not as the name of
	   an existing file. Go through various combinations of
	   unified/non-unified formatted/unformatted to find data.
	*/
	ecl_storage_enum storage_mode = ECL_INVALID_STORAGE;
	const char * eclbase = input[0];
	char * unified_file  = NULL;
	char * file1	     = NULL;
	char * file2	     = NULL;       
	
	unified_file = ecl_util_alloc_filename(NULL , eclbase , ECL_UNIFIED_RESTART_FILE , false , -1);
	if (util_file_exists( unified_file ))
	  /* Binary unified */
	  storage_mode = ECL_BINARY_UNIFIED;
	else {
	  /* Binary non-unified */
	  file1 = ecl_util_alloc_filename(NULL , eclbase , ECL_RESTART_FILE , false , report1);
	  file2 = ecl_util_alloc_filename(NULL , eclbase , ECL_RESTART_FILE , false , report2);
	  if ((util_file_exists(file1) && util_file_exists(file2))) 
	    storage_mode = ECL_BINARY_NON_UNIFIED;
	  else {
	    free(unified_file);
	    /* ASCII unified */
	    unified_file = ecl_util_alloc_filename(NULL , eclbase , ECL_UNIFIED_RESTART_FILE , true , -1);
	    if (util_file_exists( unified_file ))
	      storage_mode = ECL_FORMATTED_UNIFIED;
	    else {
	      /* ASCII non unified */
	      free(file1);
	      free(file2);
	      file1 = ecl_util_alloc_filename(NULL , eclbase , ECL_RESTART_FILE , true , report1);
	      file2 = ecl_util_alloc_filename(NULL , eclbase , ECL_RESTART_FILE , true , report2);
	      if ((util_file_exists(file1) && util_file_exists(file2))) 
		storage_mode = ECL_FORMATTED_UNIFIED;
	    }
	  }
	}

	if (storage_mode == ECL_INVALID_STORAGE) {
	  char * cwd = util_alloc_cwd();
	  util_exit("Could not find any restart information for ECLBASE:%s in %s \n", eclbase , cwd);
	  free( cwd );
	}
	
	if ((storage_mode == ECL_BINARY_UNIFIED) || (storage_mode == ECL_FORMATTED_UNIFIED)) {
	  restart_files[0] = ecl_file_fread_alloc_unrst_section( input[0] , report1 , true);
	  restart_files[1] = ecl_file_fread_alloc_unrst_section( input[0] , report2 , true);
	} else {
	  restart_files[0] = ecl_file_fread_alloc( file1 , true);
	  restart_files[1] = ecl_file_fread_alloc( file2 , true);
	}
	  


	*use_eclbase = true;
	if ((storage_mode == ECL_BINARY_UNIFIED) || (storage_mode == ECL_BINARY_NON_UNIFIED))
	  *fmt_file = false;
	else
	  *fmt_file = true;
	
	*arg_offset = 3;
	
	util_safe_free( file1 );
	util_safe_free( file2 );
	util_safe_free( unified_file );
      }
    }
  }
  return restart_files;
}

/*****************************************************************/
/* Main program                                                  */
/*****************************************************************/

int main(int argc , char ** argv) {
  
  if(argc > 1) {
    if(strcmp(argv[1], "-h") == 0)
      print_usage();
  }

  if(argc < 2)
    print_usage();


  else{
    char ** input        = &argv[1];   /* Skipping the name of the executable */
    int     input_length = argc - 1;   
    int     input_offset = 0;
    bool    use_eclbase, fmt_file; 

    
    const char * report_filen  = "RUN_GRAVITY.out"; 
    
    ecl_file_type ** restart_files;
    ecl_file_type  * init_file;
    ecl_grid_type  * ecl_grid;
    
    int model_phases;
    int file_phases;
    vector_type * grav_stations = vector_alloc_new();
    
    
    /* Restart info */
    restart_files = load_restart_info( (const char **) input , input_length , &input_offset , &use_eclbase , &fmt_file);

    
    /* INIT and GRID/EGRID files */
    {
      char           * grid_filename = NULL;
      char           * init_filename = NULL;
      if (use_eclbase) {
	/* 
	   The first command line argument is interpreted as ECLBASE, and we
	   search for grid and init files in cwd.
	*/
	init_filename = ecl_util_alloc_exfilename_anyfmt( NULL , input[0] , ECL_INIT_FILE  , fmt_file , -1);
	grid_filename = ecl_util_alloc_exfilename_anyfmt( NULL , input[0] , ECL_EGRID_FILE , fmt_file , -1);
	if (grid_filename == NULL)
	  grid_filename = ecl_util_alloc_exfilename_anyfmt( NULL , input[0] , ECL_GRID_FILE , fmt_file , -1);
	if ((init_filename == NULL) || (grid_filename == NULL))  /* Means we could not find them. */
	  print_usage();
      } else {
	/* */
	if ((input_length - input_offset) > 1) {
	  init_filename = util_alloc_string_copy(input[input_offset]);
	  grid_filename = util_alloc_string_copy(input[input_offset + 1]);
	  input_offset += 2;
	} else print_usage();
      }
      
      init_file     = ecl_file_fread_alloc(init_filename , true);
      ecl_grid      = ecl_grid_alloc(grid_filename , true);
      free( init_filename );
      free( grid_filename );
    }
    
    // station_file
    if (input_length > input_offset) {
      char * station_file = input[input_offset];
      if (util_file_exists(station_file))
	load_stations( grav_stations , station_file);
      else
	print_usage();
    } else 
      print_usage();

    /** OK - now everything is loaded */
        
    
        
    // Get the relevant kws and vectors
    // RPORV
    ecl_kw_type * rporv1_kw  ;
    ecl_kw_type * rporv2_kw ;
    
    model_phases = 0;
    if (ecl_file_has_kw(restart_files[0] , "OIL_DEN"))
      model_phases += OIL;			  	      

    if (ecl_file_has_kw(restart_files[0] , "WAT_DEN"))
      model_phases += WATER;			  	      

    if (ecl_file_has_kw(restart_files[0] , "GAS_DEN"))
      model_phases += GAS;

    /** We assume the restart file NEVER has SOIL information */
    file_phases = 0;
    if (ecl_file_has_kw(restart_files[0] , "SWAT"))
      file_phases += WATER;
    if (ecl_file_has_kw(restart_files[0] , "SGAS"))
      file_phases += GAS;

    /* Consiency check */
    {
      /**
	 The following assumptions are made:
	 
	 1. All restart files should have water, i.e. the SWAT keyword. 
	 2. All phases present in the restart file should also be present as densities, 
	    in addition the model must contain one additional phase. 
	 3. The restart files can never contain oil saturation.
	 
      */
      if (! has_phase( file_phases , WATER ) )
	util_exit("Could not locate SWAT keyword in restart files\n");
      
      if ( has_phase( file_phases , OIL ))
	util_exit("Can not handle restart files with SOIL keyword\n"); 
      
      if (! has_phase( model_phases , WATER ) )
	util_exit("Could not locate WAT_DEN keyword in restart files\n");      
      
      if ( has_phase( file_phases , GAS )) {
	/** Restart file has both water and gas - means we need all three densities. */
	if (! (has_phase(model_phases , GAS) && has_phase(model_phases , OIL)))
	  util_exit("Could not find GAS_DEN and OIL_DEN keywords in restart files\n");
      } else {
	/* This is (water + oil) or (water + gas) system. We enforce one of the densities.*/
	if ( !has_phase( model_phases , GAS + OIL))
	  util_exit("Could not find either GAS_DEN or OIL_DEN kewyords in restart files\n");
      }
    }
    
    
    
    if( ecl_file_has_kw( restart_files[0] , "RPORV") || ecl_file_has_kw( restart_files[1] , "RPORV") ){   
      rporv1_kw    = ecl_file_iget_named_kw(restart_files[0], "RPORV", 0);
      rporv2_kw    = ecl_file_iget_named_kw(restart_files[1], "RPORV", 0);
    } else {
      printf("Sorry, the restartfiles does not contain RPORV\n");      
      exit(1);
    }


    /* 
       OK - now it seems the provided files have all the information
       we need. Let us start extracting, and then subsequently using
       it.
    */
    {
      ecl_kw_type * oil_den1_kw = NULL ;  
      ecl_kw_type * oil_den2_kw = NULL ;
      ecl_kw_type * gas_den1_kw = NULL ;
      ecl_kw_type * gas_den2_kw = NULL ;
      ecl_kw_type * wat_den1_kw = NULL ;
      ecl_kw_type * wat_den2_kw = NULL ;
      ecl_kw_type * sgas1_kw 	= NULL;
      ecl_kw_type * sgas2_kw 	= NULL;
      ecl_kw_type * swat1_kw 	= NULL;
      ecl_kw_type * swat2_kw 	= NULL;
      ecl_kw_type * aquifern_kw = NULL ;

      /** Extracting the densities */
      {
	// OIL_DEN
	if( has_phase(model_phases , OIL) ) {
	  oil_den1_kw  = ecl_file_iget_named_kw(restart_files[0], "OIL_DEN", 0);
	  oil_den2_kw  = ecl_file_iget_named_kw(restart_files[1], "OIL_DEN", 0);
	}
	
	// GAS_DEN
	if( has_phase( model_phases , GAS) ) {
	  gas_den1_kw  = ecl_file_iget_named_kw(restart_files[0], "GAS_DEN", 0);
	  gas_den2_kw  = ecl_file_iget_named_kw(restart_files[1], "GAS_DEN", 0);
	}
	
	// WAT_DEN
	if( has_phase( model_phases , WATER) ) {
	  wat_den1_kw  = ecl_file_iget_named_kw(restart_files[0], "WAT_DEN", 0);
	  wat_den2_kw  = ecl_file_iget_named_kw(restart_files[1], "WAT_DEN", 0);
	}
      }
      

      /* Extracting the saturations */
      {
	// SGAS
	if( has_phase( file_phases , GAS )) {
	  sgas1_kw     = ecl_file_iget_named_kw(restart_files[0], "SGAS", 0);
	  sgas2_kw     = ecl_file_iget_named_kw(restart_files[1], "SGAS", 0);
	} 

	// SWAT
	if( has_phase( file_phases , WATER )) {
	  swat1_kw     = ecl_file_iget_named_kw(restart_files[0], "SWAT", 0);
	  swat2_kw     = ecl_file_iget_named_kw(restart_files[1], "SWAT", 0);
	} 
      }
      
      /* The numerical aquifer information */
      if( ecl_file_has_kw( init_file , "AQUIFERN")) 
	aquifern_kw     = ecl_file_iget_named_kw(init_file, "AQUIFERN", 0);
      
      {
	int     nactive  = ecl_grid_get_active_size( ecl_grid );
	float * zero     = util_malloc( nactive * sizeof * zero     , __func__);    /* Fake vector of zeros used for densities / sturations when you do not have data. */
	int   * int_zero = util_malloc( nactive * sizeof * int_zero , __func__);    /* Fake vector of zeros used for AQUIFER when the init file does not supply data. */
	{
	  int i;
	  for (i=0; i < nactive; i++) {
	    zero[i]     = 0;
	    int_zero[i] = 0;
	  }
	}
	{
	  const float * sgas1_v   = safe_get_float_ptr( sgas1_kw 	  , NULL );
	  const float * swat1_v   = safe_get_float_ptr( swat1_kw 	  , NULL );
	  const float * oil_den1  = safe_get_float_ptr( oil_den1_kw , zero );
	  const float * gas_den1  = safe_get_float_ptr( gas_den1_kw , zero );
	  const float * wat_den1  = safe_get_float_ptr( wat_den1_kw , zero );
	  
	  const float * sgas2_v   = safe_get_float_ptr( sgas2_kw 	  , NULL );
	  const float * swat2_v   = safe_get_float_ptr( swat2_kw 	  , NULL );
	  const float * oil_den2  = safe_get_float_ptr( oil_den2_kw , zero );
	  const float * gas_den2  = safe_get_float_ptr( gas_den2_kw , zero );
	  const float * wat_den2  = safe_get_float_ptr( wat_den2_kw , zero );
	  
	  const float * rporv1    = ecl_kw_get_float_ptr(rporv1_kw);
	  const float * rporv2    = ecl_kw_get_float_ptr(rporv2_kw);
	  int   * aquifern;
	  int global_index;
	  
	  if (aquifern_kw != NULL)
	    aquifern = ecl_kw_get_int_ptr( aquifern_kw );
	  else
	    aquifern = int_zero;
	  
	  
	  for (global_index=0;global_index < ecl_grid_get_global_size( ecl_grid ); global_index++){
	    const int act_index = ecl_grid_get_active_index1( ecl_grid , global_index );
	    if (act_index >= 1) {
	      // Not numerical aquifer 
	      if(aquifern[act_index] >= 0){ 
		float swat1 = swat1_v[act_index];
		float swat2 = swat2_v[act_index];
		float sgas1 = 0;
		float sgas2 = 0;
		float soil1 = 0;
		float soil2 = 0;
		
		truncate_saturation( &swat1 );
		truncate_saturation( &swat2 );
		
		if (has_phase( model_phases , GAS)) {
		  if (has_phase( file_phases , GAS )) {
		    sgas1 = sgas1_v[act_index];
		    sgas2 = sgas2_v[act_index];
		    truncate_saturation( &sgas1 );
		    truncate_saturation( &sgas2 );
		  } else {
		    sgas1 = 1 - swat1;
		    sgas2 = 1 - swat2;
		  }
		}
		
		if (has_phase( model_phases , OIL )) {
		  soil1 =  1 - sgas1  - swat1;
		  soil2 =  1 - sgas2  - swat2;
		  truncate_saturation( &soil1 );
		  truncate_saturation( &soil2 );
		}
		
		//printf ("SOIL1 : %f\t",soil1);
		//printf ("SGAS1 : %f\t",sgas1);
		//printf ("SWAT1 : %f\n",swat1);
		//
		//printf ("SOIL2 : %f\t",soil2);
		//printf ("SGAS2 : %f\t",sgas2);
		//printf ("SWAT2 : %f\n",swat2);
		//printf ("DEN_GAS1 : %f\n",gas_den1[act_index]);
		//printf ("DEN_GAS2 : %f\n",gas_den2[act_index]);
		
		//if(swat1[act_index] < 0.999 && swat2[act_index] < 0.999){	  // Check if this is an aquifer cell; neglect these
		

	
		/* 
		   We have found all the info we need for one cell, then we loop over all the grav
		   stations and calculate the delta G from this cell for the various stations.
		*/
		{
		  double  mas1 , mas2;
		  double  xpos , ypos , zpos;
		  int     station_nr;

		  mas1 = rporv1[act_index]*(soil1 * oil_den1[act_index] + sgas1 * gas_den1[act_index] + swat1 * wat_den1[act_index] );
		  mas2 = rporv2[act_index]*(soil2 * oil_den2[act_index] + sgas2 * gas_den2[act_index] + swat2 * wat_den2[act_index] );
		  //if(!(mas1>=0) || !(mas2>=0)){
		  //  printf("Cell index: %i  %i %i %i %i \n", act_index, global_index, i, j, k);
		  //  printf("mas1: %f mas2: %f %i\n", mas1, mas2, act_index);
		  //  printf("%f %f %f %f\n", rporv1[act_index], oil_den1[act_index], gas_den1[act_index], wat_den1[act_index] );
		  //  printf("%f %f %f %f\n", rporv2[act_index], oil_den2[act_index], gas_den2[act_index], wat_den2[act_index] );
		  //  exit(1);
		  //}
		  
		  
		  ecl_grid_get_pos1(ecl_grid , global_index , &xpos , &ypos , &zpos);
		  for(station_nr=0; station_nr < vector_get_size( grav_stations ); station_nr++) {
		    grav_station_type * g_s = vector_iget(grav_stations, station_nr);
		    double dist_x  = xpos - g_s->utm_x;
		    double dist_y  = ypos - g_s->utm_y;
		    double dist_d  = zpos - g_s->depth;
		    double dist_sq = dist_x*dist_x + dist_y*dist_y + dist_d*dist_d;
		    double ldelta_g;

		    if(dist_sq == 0){
		      exit(1);
		    }
		    ldelta_g = 6.67E-3*(mas2 - mas1)*dist_d/pow(dist_sq, 1.5);
		    grav_diff_update(g_s , ldelta_g);
		    //printf("DIST %f \n",dist_sq);
		    //printf ("DELTA_G: %f %f\n", g_s->grav_diff, ldelta_g);
		  }
		}
	      }
	    }
	  }
	}
	free( zero );
	free( int_zero );
      }
    }
    
    {
      FILE * stream = util_fopen(report_filen , "w");
      
      int station_nr;
      for(station_nr = 0; station_nr < vector_get_size( grav_stations ); station_nr++){
	const grav_station_type * g_s = vector_iget_const(grav_stations, station_nr);
	fprintf(stream, "%f\n",g_s->grav_diff);
	printf ("DELTA_G %i: %f %f %f %f\n", station_nr, g_s->grav_diff, g_s->utm_x, g_s->utm_y, g_s->depth);
      }
      fclose(stream);
    }
    

    // Clean up the mess 
    vector_free( grav_stations );
    ecl_grid_free(ecl_grid);
    ecl_file_free(restart_files[0]);
    ecl_file_free(restart_files[1]);
    free( restart_files );
    ecl_file_free(init_file);
  }		
}
