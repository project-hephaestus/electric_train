/** $Id: windturb_dg.cpp,v 1.0 2011/02/24 sing463 Exp $
	Copyright (C) 2008 Battelle Memorial Institute
	
	@file windturb_dg.cpp
	@defgroup windturb_dg Wind Turbine gensets
	@ingroup generators

 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <complex.h>

#include "windturb_dg.h"

CLASS *windturb_dg::oclass = NULL;
windturb_dg *windturb_dg::defaults = NULL;
static PASSCONFIG clockpass = PC_BOTTOMUP;

windturb_dg::windturb_dg(MODULE *module)
{
	if (oclass==NULL)
	{
		// register to receive notice for first top down. bottom up, and second top down synchronizations
		oclass = gl_register_class(module,"windturb_dg",sizeof(windturb_dg),PC_PRETOPDOWN|PC_BOTTOMUP);
		if (oclass==NULL)
			throw "unable to register class windturb_dg";
		else
			

		if (gl_publish_variable(oclass,
			// TO DO:  publish your variables here
			PT_enumeration,"Gen_status",PADDR(Gen_status),
				PT_KEYWORD,"OFFLINE",OFFLINE,
				PT_KEYWORD,"ONLINE",ONLINE,
			PT_enumeration,"Gen_type",PADDR(Gen_type),
				PT_KEYWORD,"INDUCTION",INDUCTION,
				PT_KEYWORD,"SYNCHRONOUS",SYNCHRONOUS,
			PT_enumeration,"Gen_mode",PADDR(Gen_mode),
				PT_KEYWORD,"CONSTANTE",CONSTANTE,
				PT_KEYWORD,"CONSTANTP",CONSTANTP,
				PT_KEYWORD,"CONSTANTPQ",CONSTANTPQ,
			PT_enumeration,"Turbine_Model",PADDR(Turbine_Model),
				PT_KEYWORD,"GENERIC_SYNCH_SMALL",GENERIC_SYNCH_SMALL,
				PT_KEYWORD,"GENERIC_SYNCH_MID",GENERIC_SYNCH_MID,
				PT_KEYWORD,"GENERIC_SYNCH_LARGE",GENERIC_SYNCH_LARGE,
				PT_KEYWORD,"GENERIC_IND_SMALL",GENERIC_IND_SMALL,
				PT_KEYWORD,"GENERIC_IND_MID",GENERIC_IND_MID,
				PT_KEYWORD,"GENERIC_IND_LARGE",GENERIC_IND_LARGE,
                PT_KEYWORD,"USER_DEFINED",USER_DEFINED,  // Doesnt work yet
				PT_KEYWORD,"VESTAS_V82",VESTAS_V82,
				PT_KEYWORD,"GE_25MW",GE_25MW,
			
		    PT_double, "turbine_height", PADDR(turbine_height),
			PT_double, "blade_diam", PADDR(blade_diam),
            PT_double, "cut_in_ws", PADDR(cut_in_ws),
            PT_double, "cut_out_ws", PADDR(cut_out_ws),
            PT_double, "ws_rated", PADDR(ws_rated),
            PT_double, "ws_maxcp", PADDR(ws_maxcp),
            PT_double, "Cp_max", PADDR(Cp_max),
			PT_double, "Cp_rated", PADDR(Cp_rated),
       		PT_double, "Rated_VA[VA]", PADDR(Rated_VA),
			PT_double, "Rated_V[V]", PADDR(Rated_V),
			PT_double, "Pconv[W]", PADDR(Pconv),			
			PT_double, "WSadj[m/s]", PADDR(WSadj),
			PT_double, "Wind_Speed[m/s]", PADDR(Wind_Speed), // rated wind speed
			//PT_double, "Gear_ratio", PADDR(Gear_ratio),
			
			PT_double, "R_stator", PADDR(Rst),
			PT_double, "X_stator", PADDR(Xst),
			PT_double, "R_rotor", PADDR(Rr),
			PT_double, "X_rotor", PADDR(Xr),
			PT_double, "R_core", PADDR(Rc),
			PT_double, "X_magnetic", PADDR(Xm),
			PT_double, "Max_Vrotor", PADDR(Max_Vrotor),
			PT_double, "Min_Vrotor", PADDR(Min_Vrotor),
			PT_double, "Max_Ef", PADDR(Max_Ef),
			PT_double, "Min_Ef", PADDR(Min_Ef),
			PT_double, "Rs", PADDR(Rs),
			PT_double, "Xs", PADDR(Xs),
			PT_double, "Rg", PADDR(Rg),
			PT_double, "Xg", PADDR(Xg),
			PT_double, "pf", PADDR(pf),

			PT_double, "GenElecEff", PADDR(GenElecEff),
			PT_double, "TotalRealPow[W]", PADDR(TotalRealPow),
			PT_double, "TotalReacPow", PADDR(TotalReacPow),
				
			PT_complex, "voltage_A[V]", PADDR(voltage_A),
			PT_complex, "voltage_B[V]", PADDR(voltage_B),
			PT_complex, "voltage_C[V]", PADDR(voltage_C),
			PT_complex, "current_A[A]", PADDR(current_A),
			PT_complex, "current_B[A]", PADDR(current_B),
			PT_complex, "current_C[A]", PADDR(current_C),
			PT_complex, "EfA[V]", PADDR(EfA),			//Synchronous Generator
			PT_complex, "EfB[V]", PADDR(EfB),
			PT_complex, "EfC[V]", PADDR(EfC),
			PT_complex, "Vrotor_A[V]", PADDR(Vrotor_A), //Induction Generator
			PT_complex, "Vrotor_B[V]", PADDR(Vrotor_B),
			PT_complex, "Vrotor_C[V]", PADDR(Vrotor_C),
			PT_complex, "Irotor_A[V]", PADDR(Irotor_A),
			PT_complex, "Irotor_B[V]", PADDR(Irotor_B),
			PT_complex, "Irotor_C[V]", PADDR(Irotor_C),

			PT_set, "phases", PADDR(phases),
				PT_KEYWORD, "A",(set)PHASE_A,
				PT_KEYWORD, "B",(set)PHASE_B,
				PT_KEYWORD, "C",(set)PHASE_C,
				PT_KEYWORD, "N",(set)PHASE_N,
				PT_KEYWORD, "S",(set)PHASE_S,
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
		
		
	}
}

/* Object creation is called once for each object that is created by the core */
int windturb_dg::create(void) 
{
	
	// Defaults usually inside the create function
		
	roughness_l = .055;		//European wind atlas def for terrain roughness, values range from 0.0002 to 1.6
	ref_height = 10;		//height wind data was measured (Most meteorological data taken at 5-15 m)
	Max_Vrotor = 1.2;
	Min_Vrotor = 0.8;
	Max_Ef = 1.2;
	Min_Ef = 0.8;
	avg_ws = 8;				//wind speed in m/s

	time_advance = 3600;	//amount of time to advance for WS data import in secs.

	/* set the default values of all properties here */
	Ridealgas = 8.31447;
	Molar = 0.0289644;
	std_air_dens = 1.2754;	//dry air density at std pressure and temp in kg/m^3
	std_air_temp = 0;	//IUPAC std air temp in Celsius
	std_air_press = 100000;	//IUPAC std air pressure in Pascals
	Turbine_Model = GENERIC_SYNCH_LARGE;	//************** Default specified so it doesn't crash out
	Gen_mode = CONSTANTP;					//************** Default specified so values actually come out
	Gen_status = ONLINE;
 
	turbine_height = -9999;
	blade_diam = -9999;
	cut_in_ws = -9999;
	cut_out_ws = -9999;
	Cp_max = -9999;
	Cp_rated =-9999;
	ws_maxcp = -9999;
	ws_rated = -9999;
	CP_Data = CALCULATED;

	return 1; /* return 1 on success, 0 on failure */
}

/** Checks for climate object and maps the climate variables to the windturb object variables.  
If no climate object is linked, then default pressure, temperature, and wind speed will be used.
**/
int windturb_dg::init_climate()
{
	OBJECT *hdr = OBJECTHDR(this);

	// link to climate data
	static FINDLIST *climates = NULL;
	int not_found = 0;
	
	climates = gl_find_objects(FL_NEW,FT_CLASS,SAME,"climate",FT_END);
	if (climates==NULL)
	{
		not_found = 1;
		gl_warning("windturb_dg: no climate data found, using static data");

		//default to mock data
		static double air_dens = std_air_dens, avgWS = avg_ws, Press = std_air_press, Temp = std_air_temp;
		//pPress = &Press;
		//pTemp = &Temp;
		pWS = &avgWS;
				
	}
	else if (climates->hit_count>1)
	{
		gl_warning("windturb_dg: %d climates found, using first one defined", climates->hit_count);
	}
	// }
	if (climates!=NULL)	
	{
		if (climates->hit_count==0)	
		{
			//default to mock data
			gl_warning("windturb_dg: no climate data found, using mock data");
			static double air_dens = std_air_dens, avgWS = avg_ws, Press = std_air_press, Temp = std_air_temp;
			//pPress = &Press;
			//pTemp = &Temp;
			pWS = &avgWS;
	}
		else //climate data was found
		{
			// force rank of object w.r.t climate
			gl_warning("windturb_dg: using climate data");
			OBJECT *obj = gl_find_next(climates,NULL);
			if (obj->rank<=hdr->rank)
				gl_set_dependent(obj,hdr);
			pWS = (double*)GETADDR(obj,gl_get_property(obj,"wind_speed"));  

			//Make sure it worked *********** added ***************
			if (pWS==NULL)
			{
				GL_THROW("Unable to map wind_speed from climate object");
			}
			//pPress = (double*)GETADDR(obj,gl_get_property(obj,"pressure"));  //TO DO:  add atm press into climate models
			static double Press = std_air_press;
			pPress = &Press;
		}
	}

	

	return 1;
}

int windturb_dg::init(OBJECT *parent)
{
	double ZB, SB, EB;
	complex tst, tst2, tst3, tst4;

	switch (Turbine_Model)	{
		case GENERIC_IND_LARGE:
			 break;

        case GENERIC_IND_MID:
			break;

	    case GENERIC_IND_SMALL:		
             break;

		case GENERIC_SYNCH_LARGE:	//Creates generic 1.5 MW wind turbine.
			blade_diam = 82.5;
			turbine_height = 90;
			q = 3;						//number of gearbox stages
			Rated_VA = 1635000;
			Max_P = 1500000;
			Max_Q = 650000;
			Rated_V = 600;
			pf = 0.95;
			CP_Data = GENERAL_LARGE;
				cut_in_ws = 4;			//lowest wind speed 
				cut_out_ws = 25;		//highest wind speed 
				Cp_max = 0.302;			//rotor specifications for power curve
				ws_maxcp = 7;			
				Cp_rated = Cp_max-.05;	
				ws_rated = 12.5;		
			if (Turbine_Model == GENERIC_IND_LARGE) {
				Gen_type = INDUCTION;
				Rst = 0.12;					
				Xst = 0.17;					
				Rr = 0.12;				
				Xr = 0.15;			
				Rc = 999999;		
				Xm = 9.0;	
			}
			else if (Turbine_Model == GENERIC_SYNCH_LARGE) {
				Gen_type = SYNCHRONOUS;
				Rs = 0.05;
				Xs = 0.200;
				Rg = 0.000;
				Xg = 0.000;
			}
			break;

		case GENERIC_SYNCH_MID:	//Creates generic 100kW wind turbine, northwind 100
			blade_diam = 23.2;   //in m
			turbine_height = 30;   //in m
			q = 0;						//number of gearbox stages, no gear box 
			Rated_VA = 156604;
			Max_P = 150000;
			Max_Q = 45000;
			Rated_V = 480;  
			pf = 0.9;     ///lag and lead of 0.9
			CP_Data = GENERAL_MID;
				cut_in_ws = 3.5;			//lowest wind speed in m/s
				cut_out_ws = 25;		   //highest wind speed in m/s
				Cp_max = 0.302;			  //rotor specifications for power curve
				ws_maxcp = 7;			
				Cp_rated = Cp_max-.05;	
				ws_rated = 14.5;		//	in m/s
			if (Turbine_Model == GENERIC_IND_MID) {     // need to check the machine parameters
				Gen_type = INDUCTION;
				Rst = 0.12;					
				Xst = 0.17;					
				Rr = 0.12;				
				Xr = 0.15;			
				Rc = 999999;		
				Xm = 9.0;	
			}
			else if (Turbine_Model == GENERIC_SYNCH_MID) {
				Gen_type = SYNCHRONOUS;
				Rs = 0.05;
				Xs = 0.200;
				Rg = 0.000;
				Xg = 0.000;
			}
			break;
							
        case GENERIC_SYNCH_SMALL:	//Creates generic 5 kW wind turbine, Fortis Montana 5 kW wind turbine
			blade_diam = 5;      // in m
			turbine_height = 16;   //in m
			q = 0;						//number of gearbox stages, no gear box
			Rated_VA = 6315;               // calculate from P & Q
			Max_P = 5800; 
			Max_Q = 2500;
			Rated_V = 600;
			pf = 0.95;
			CP_Data = GENERAL_SMALL;
				cut_in_ws = 2.5;			//lowest wind speed 
				cut_out_ws = 25;		//highest wind speed 
				Cp_max = 0.302;			//rotor specifications for power curve
				ws_maxcp = 7;			//	|
				Cp_rated = Cp_max-.05;	//	|
				ws_rated = 17;		//	|
			if (Turbine_Model == GENERIC_IND_SMALL) {
				Gen_type = INDUCTION;
				Rst = 0.12;					
				Xst = 0.17;					
				Rr = 0.12;				
				Xr = 0.15;			
				Rc = 999999;		
				Xm = 9.0;	
			}
			else if (Turbine_Model == GENERIC_SYNCH_SMALL) {
				Gen_type = SYNCHRONOUS;
				Rs = 0.05;
				Xs = 0.200;
				Rg = 0.000;
				Xg = 0.000;
			}

			break;

		case VESTAS_V82:	//Include manufacturer's data - cases can be added to call other wind turbines
			turbine_height = 78;
			blade_diam = 82;
			Rated_VA = 1808000;
			Rated_V = 600;
			Max_P = 1650000;
			Max_Q = 740000;
			pf = 0.91;		//Can range between 0.65-1.00 depending on controllers and Pout.
			CP_Data = MANUF_TABLE;		
				cut_in_ws = 3.5;
				cut_out_ws = 20;
			q = 2;
			Gen_type = SYNCHRONOUS;	//V82 actually uses a DFIG, but will use synch representation for now
				Rs = 0.025;				//Estimated values for synch representation.
				Xs = 0.200;
				Rg = 0.000;
				Xg = 0.000;
			break;

		case GE_25MW:
			turbine_height = 100;
			blade_diam = 100;
			Rated_VA = 2727000;
			Rated_V = 690;
			Max_P = 2500000;
			Max_Q = 1090000;
			pf = 0.95;		//ranges between -0.9 -> 0.9;
			q = 3;
			CP_Data = GENERAL_LARGE;
				cut_in_ws = 3.5;
				cut_out_ws = 25;
				Cp_max = 0.28;
				Cp_rated = 0.275;
				ws_maxcp = 8.2;
				ws_rated = 12.5;
			Gen_type = SYNCHRONOUS;
				Rs = 0.035;
				Xs = 0.200;
				Rg = 0.000;
				Xg = 0.000;
			break;

        case USER_DEFINED:
		
			CP_Data = USER_SPECIFY;	

			Gen_type = USER_TYPE;
				Rs = 0.2;
				Xs = 0.2;
				Rg = 0.1;
				Xg = 0;

				if (turbine_height <=0)
					GL_THROW ("turbine height cannot have a negative or zero value.");

               if (blade_diam <=0)
					GL_THROW ("blade diameter cannot have a negative or zero value.");

			   if (cut_in_ws <=0)
					GL_THROW ("cut in wind speed cannot have a negative or zero value.");
                
			  if (cut_out_ws <=0)
					GL_THROW ("cut out wind speed cannot have a negative or zero value.");

			  if (ws_rated <=0)
					GL_THROW ("rated wind speed cannot have a negative or zero value.");
			   
			   if (ws_maxcp <=0)
					GL_THROW ("max cp  cannot have a negative or zero value.");


		
			break;

		default:
			GL_THROW("Unknown Turbine Model");
			/*  TROUBLESHOOT
			An unknown wind turbine model was selected
			*/
		
	}

	// construct circuit variable map to meter -- copied from 'House' module
	struct {
		complex **var;
		char *varname;
	} map[] = {
		// local object name,	meter object name
		{&pCircuit_V,			"voltage_A"}, // assumes 2 and 3 follow immediately in memory
		{&pLine_I,				"current_A"}, // assumes 2 and 3(N) follow immediately in memory
		/// @todo use triplex property mapping instead of assuming memory order for meter variables (residential, low priority) (ticket #139)
	};

	static complex default_line123_voltage[3], default_line1_current[3];
	int i;

	// find parent meter, if not defined, use a default meter (using static variable 'default_meter')
	if (parent!=NULL)
	{
		if (gl_object_isa(parent,"meter","powerflow"))	//Attach to meter
		{
			// attach meter variables to each circuit
			for (i=0; i<sizeof(map)/sizeof(map[0]); i++)
			{
				*(map[i].var) = get_complex(parent,map[i].varname);
				
				if (*(map[i].var) == NULL)
				{
					GL_THROW("Unable to map variable %s",map[i].varname);
					/*  TROUBLESHOOT
					The variable name was not found when mapping it
					*/
				}
			}
		}
		else if (gl_object_isa(parent,"triplex_meter","powerflow"))
		{
			//Map voltage
			pCircuit_V = get_complex(parent,"voltage_1");

			//Make sure it worked
			if (pCircuit_V == NULL)
				GL_THROW("Unable to map triplex_meter voltage");

			//Map current
			pLine_I = get_complex(parent,"current_1");
			
			//Make sure it worked
			if (pLine_I == NULL)
				GL_THROW("Unable to map triplex_meter current");
		}
		else
		{
			GL_THROW("Invalid parent object");
			/* TROUBLESHOOT */
		}
	}
	else
	{
		OBJECT *obj = OBJECTHDR(this);
		gl_warning("windturb_dg:%d %s", obj->id, parent==NULL?"has no parent meter defined":"parent is not a meter");	

		// attach meter variables to each circuit in the default_meter
			*(map[0].var) = &default_line123_voltage[0];
			*(map[1].var) = &default_line1_current[0];

		// provide initial values for voltages
			default_line123_voltage[0] = complex(Rated_V/sqrt(3.0),0);
			default_line123_voltage[1] = complex(Rated_V/sqrt(3.0)*cos(2*PI/3),Rated_V/sqrt(3.0)*sin(2*PI/3));
			default_line123_voltage[2] = complex(Rated_V/sqrt(3.0)*cos(-2*PI/3),Rated_V/sqrt(3.0)*sin(-2*PI/3));

	}


	if (Gen_status==OFFLINE)
	{
		GL_THROW("Generator is out of service!"); 	}
	
	else
	{	
		if (Rated_VA!=0.0)  SB = Rated_VA/3;
		if (Rated_V!=0.0)  EB = Rated_V/sqrt(3.0);
		if (SB!=0.0)  ZB = EB*EB/SB;
		else GL_THROW("Generator power capacity not specified!");
	}
		
	if (Gen_type == INDUCTION)  
	{
		complex Zrotor(Rr,Xr);
		complex Zmag = complex(Rc*Xm*Xm/(Rc*Rc + Xm*Xm),Rc*Rc*Xm/(Rc*Rc + Xm*Xm));
		complex Zstator(Rst,Xst);

		//Induction machine two-port matrix.
		IndTPMat[0][0] = (Zmag + Zstator)/Zmag;
		IndTPMat[0][1] = Zrotor + Zstator + Zrotor*Zstator/Zmag;
		IndTPMat[1][0] = complex(1,0) / Zmag;
		IndTPMat[1][1] = (Zmag + Zrotor) / Zmag;
	}

	else if (Gen_type == SYNCHRONOUS)  
	{
		double Real_Rs = Rs * ZB; 
		double Real_Xs = Xs * ZB;
		double Real_Rg = Rg * ZB; 
		double Real_Xg = Xg * ZB;
		tst = complex(Real_Rg,Real_Xg);
		tst2 = complex(Real_Rs,Real_Xs);
		AMx[0][0] = tst2 + tst;			//Impedance matrix
		AMx[1][1] = tst2 + tst;
		AMx[2][2] = tst2 + tst;
		AMx[0][1] = AMx[0][2] = AMx[1][0] = AMx[1][2] = AMx[2][0] = AMx[2][1] = tst;
		tst3 = (complex(1,0) + complex(2,0)*tst/tst2)/(tst2 + complex(3,0)*tst);
		tst4 = (-tst/tst2)/(tst2 + tst);
		invAMx[0][0] = tst3;			//Admittance matrix (inverse of Impedance matrix)
		invAMx[1][1] = tst3;
		invAMx[2][2] = tst3;
		invAMx[0][1] = AMx[0][2] = AMx[1][0] = AMx[1][2] = AMx[2][0] = AMx[2][1] = tst4;
	}

	init_climate();

return 1;
}

// Presync is called when the clock needs to advance on the first top-down pass */
TIMESTAMP windturb_dg::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	double Pwind, Pmech, Cp, detCp, F, G, gearbox_eff;
	double matCp[2][3];

	double air_dens = std_air_dens;//*pPress * Molar / (Ridealgas * (*pTemp + 273.15));
	
	
	//wind speed at height of hub - uses European Wind Atlas method
	WSadj = *pWS * log(turbine_height/roughness_l)/log(ref_height/roughness_l); 

	/*TO DO:  import previous and future wind data 
	and then pseudo-randomize the wind speed values beween 1st and 2nd
	WSadj = gl_pseudorandomvalue(RT_RAYLEIGH,&c,(WS1/sqrt(PI/2)));*/
	//WSadj = 1;


	Pwind = 0.5 * (air_dens) * PI * pow(blade_diam/2,2) * pow(WSadj,3);
	
	if (CP_Data == GENERAL_LARGE ||CP_Data == GENERAL_MID ||CP_Data == GENERAL_SMALL)	
	{
		if (WSadj <= cut_in_ws)
		{	
			Cp = 0;	
		}
	
		else if (WSadj > cut_out_ws)
		{	
			Cp = 0;	
		}
	
		else if(WSadj > ws_rated)
		{
			Cp = Cp_rated * pow(ws_rated,3) / pow(WSadj,3);	
		}
		else
		{   
			if (WSadj = 0 || WSadj <= cut_in_ws || WSadj >= cut_out_ws)
				{
					Cp = 0;
				}
			else {
                matCp[0][0] = pow((ws_maxcp/cut_in_ws - 1),2);   //Coeff of Performance found 
				matCp[0][1] = pow((ws_maxcp/cut_in_ws - 1),3);	 //by using method described in [1]
				matCp[0][2] = 1;
				matCp[1][0] = pow((ws_maxcp/ws_rated - 1),2);
				matCp[1][1] = pow((ws_maxcp/ws_rated - 1),3);
				matCp[1][2] = 1 - Cp_rated/Cp_max;
				detCp = matCp[0][0]*matCp[1][1] - matCp[0][1]*matCp[1][0];

				F = (matCp[0][2]*matCp[1][1] - matCp[0][1]*matCp[1][2])/detCp;
				G = (matCp[0][0]*matCp[1][2] - matCp[0][2]*matCp[1][0])/detCp;
				
			}
		}
	}
		
	
	
	
	else if (CP_Data == MANUF_TABLE)	//Coefficient of Perfomance generated from Manufacturer's table
	{
		switch (Turbine_Model)	{
			case VESTAS_V82:
				if (WSadj <= cut_in_ws || WSadj >= cut_out_ws)
				{	
					Cp = 0;	}
				else  //TO DO:  possibly replace polynomial with spline library function interpolation
				{	  
					//Uses a centered, 10th-degree polynomial Matlab interpolation of original Manuf. data
					double z = (WSadj - 10.5)/5.9161;

					//Original data [0 0 0 0 0.135 0.356 0.442 0.461 .458 .431 .397 .349 .293 .232 .186 .151 .125 .104 .087 .074 .064] from 4-20 m/s
			Cp = -0.08609*pow(z,10) + 0.078599*pow(z,9) + 0.50509*pow(z,8) - 0.45466*pow(z,7) - 0.94154*pow(z,6) + 0.77922*pow(z,5) + 0.59082*pow(z,4) - 0.23196*pow(z,3) - 0.25009*pow(z,2) - 0.24282*z + 0.37502;
				}

			break;
			default:
			GL_THROW("Coefficient of Performance model not determined.");
		}

	}
	else if (CP_Data == CALCULATED)
		{
				matCp[0][0] = pow((ws_maxcp/cut_in_ws - 1),2);   //Coeff of Performance found 
				matCp[0][1] = pow((ws_maxcp/cut_in_ws - 1),3);	 //by using method described in [1]
				matCp[0][2] = 1;
				matCp[1][0] = pow((ws_maxcp/ws_rated - 1),2);
				matCp[1][1] = pow((ws_maxcp/ws_rated - 1),3);
				matCp[1][2] = 1 - Cp_rated/Cp_max;
				detCp = matCp[0][0]*matCp[1][1] - matCp[0][1]*matCp[1][0];

				F = (matCp[0][2]*matCp[1][1] - matCp[0][1]*matCp[1][2])/detCp;
				G = (matCp[0][0]*matCp[1][2] - matCp[0][2]*matCp[1][0])/detCp;

				Cp = Cp_max*(1 - F*pow((ws_maxcp/WSadj - 1),2) - G*pow((ws_maxcp/WSadj - 1),3));
			}
	else
	{
		GL_THROW("CP_Data not defined.");
	}
	
	/// define user defined data , also catch for future cases

	Pmech = Pwind * Cp; 
				
	if (Pmech != 0)
	{
		gearbox_eff = 1 - (q*.01*Rated_VA / Pmech);	 //Method described in [2].	
		
		if (gearbox_eff < .1)		
		{
			gearbox_eff = .1;	//Prevents efficiency from becoming negative at low power.
		}							
	
		Pmech = Pwind * Cp * gearbox_eff;
	}	
		
	Pconv = 0.9 * Pmech;  //Assuming 1 % losses due to friction and windage loss and miscellaneous losses

	current_A = current_B = current_C = 0.0;

	return TS_NEVER; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}					//Comment implies something, despite TS_NEVER is all that is changed?


TIMESTAMP windturb_dg::sync(TIMESTAMP t0, TIMESTAMP t1) 
{
	int k;
	voltage_A = pCircuit_V[0];	//Syncs the meter parent to the generator.
	voltage_B = pCircuit_V[1];
	voltage_C = pCircuit_V[2];

	double Pconva = (voltage_A.Mag() / (voltage_A.Mag() + voltage_B.Mag() + voltage_C.Mag()))*Pconv;
	double Pconvb = (voltage_B.Mag() / (voltage_A.Mag() + voltage_B.Mag() + voltage_C.Mag()))*Pconv;
	double Pconvc = (voltage_C.Mag() / (voltage_A.Mag() + voltage_B.Mag() + voltage_C.Mag()))*Pconv;

	if (Gen_type == INDUCTION)	//TO DO:  Induction gen. Ef not working correctly yet.
	{
		Pconva = Pconva/Rated_VA;					//induction generator solved in pu
		Pconvb = Pconvb/Rated_VA;
		Pconvc = Pconvc/Rated_VA;
		
		Vapu = voltage_A/(Rated_V/sqrt(3.0));	
		Vbpu = voltage_B/(Rated_V/sqrt(3.0));
		Vcpu = voltage_C/(Rated_V/sqrt(3.0));

		Vrotor_A = Vapu;  
		Vrotor_B = Vbpu;
		Vrotor_C = Vcpu;

		complex detTPMat = IndTPMat[1][1]*IndTPMat[0][0] - IndTPMat[1][0]*IndTPMat[0][1];

	
		switch (Gen_mode)	{
			case CONSTANTE:
				for(k = 0; k < 6; k++)
				{
					Irotor_A = (~((complex(Pconva,0)/Vrotor_A)));
					Irotor_B = (~((complex(Pconvb,0)/Vrotor_B)));
					Irotor_C = (~((complex(Pconvc,0)/Vrotor_C)));

					Iapu = IndTPMat[1][0]*Vrotor_A + IndTPMat[1][1]*Irotor_A;
					Ibpu = IndTPMat[1][0]*Vrotor_B + IndTPMat[1][1]*Irotor_B;
					Icpu = IndTPMat[1][0]*Vrotor_C + IndTPMat[1][1]*Irotor_C;

					Vrotor_A = complex(1,0)/detTPMat * (IndTPMat[1][1]*Vapu - IndTPMat[0][1]*Iapu);
					Vrotor_B = complex(1,0)/detTPMat * (IndTPMat[1][1]*Vbpu - IndTPMat[0][1]*Ibpu);
					Vrotor_C = complex(1,0)/detTPMat * (IndTPMat[1][1]*Vcpu - IndTPMat[0][1]*Icpu);

					Vrotor_A = Vrotor_A * Max_Vrotor / Vrotor_A.Mag();
					Vrotor_B = Vrotor_B * Max_Vrotor / Vrotor_B.Mag();
					Vrotor_C = Vrotor_C * Max_Vrotor / Vrotor_C.Mag();
				}
				break;

			case CONSTANTPQ:
				for(k = 0; k < 6; k++)
				{
					Irotor_A = -(~complex(-Pconva/Vrotor_A.Mag()*cos(Vrotor_A.Arg()),Pconva/Vrotor_A.Mag()*sin(Vrotor_A.Arg())));
					Irotor_B = -(~complex(-Pconvb/Vrotor_B.Mag()*cos(Vrotor_B.Arg()),Pconvb/Vrotor_B.Mag()*sin(Vrotor_B.Arg())));
					Irotor_C = -(~complex(-Pconvc/Vrotor_C.Mag()*cos(Vrotor_C.Arg()),Pconvc/Vrotor_C.Mag()*sin(Vrotor_C.Arg())));

					Iapu = IndTPMat[1][0]*Vrotor_A - IndTPMat[1][1]*Irotor_A;
					Ibpu = IndTPMat[1][0]*Vrotor_B - IndTPMat[1][1]*Irotor_B;
					Icpu = IndTPMat[1][0]*Vrotor_C - IndTPMat[1][1]*Irotor_C;

					Vrotor_A = complex(1,0)/detTPMat * (IndTPMat[1][1]*Vapu - IndTPMat[0][1]*Iapu);
					Vrotor_B = complex(1,0)/detTPMat * (IndTPMat[1][1]*Vbpu - IndTPMat[0][1]*Ibpu);
					Vrotor_C = complex(1,0)/detTPMat * (IndTPMat[1][1]*Vcpu - IndTPMat[0][1]*Icpu);
				}
			break;
			
			default:
				GL_THROW("Unknown generator mode");
		}
	
		current_A = Iapu * Rated_VA/(Rated_V/sqrt(3.0));	
		current_B = Ibpu * Rated_VA/(Rated_V/sqrt(3.0));	
		current_C = Icpu * Rated_VA/(Rated_V/sqrt(3.0));	
	}
	
	else if (Gen_type == SYNCHRONOUS)			//synch gen is NOT solved in pu
	{											//sg ef mode is not working yet
		double Mxef, Mnef, PoutA, PoutB, PoutC, QoutA, QoutB, QoutC;
		complex SoutA, SoutB, SoutC;
		complex lossesA, lossesB, lossesC;

		Mxef = Max_Ef * Rated_V/sqrt(3.0);
		Mnef = Min_Ef * Rated_V/sqrt(3.0);


		if (Gen_mode == CONSTANTE)	//Ef is controllable to give a needed power output.
		{
			current_A = invAMx[0][0]*(voltage_A - EfA) + invAMx[0][1]*(voltage_B - EfB) + invAMx[0][2]*(voltage_C - EfC);
			current_B = invAMx[1][0]*(voltage_A - EfA) + invAMx[1][1]*(voltage_B - EfB) + invAMx[1][2]*(voltage_C - EfC);
			current_C = invAMx[2][0]*(voltage_A - EfA) + invAMx[2][1]*(voltage_B - EfB) + invAMx[2][2]*(voltage_C - EfC);

			SoutA = -voltage_A * (~(current_A));  //TO DO:  unbalanced
			SoutB = -voltage_B * (~(current_B));
			SoutC = -voltage_C * (~(current_C));

		}

		else if (Gen_mode == CONSTANTP)	//Gives a constant output power of real power converted Pout,then Qout is found through a controllable power factor.
		{									
			if (Pconva > 1.05*Max_P/3) {
				Pconva = 1.05*Max_P/3;		//If air density increases, power extracted can be much greater than amount the generator can handle.  This limits to 5% overpower.
			}								
			if (Pconvb > 1.05*Max_P/3) {
				Pconvb = 1.05*Max_P/3;
			}
			if (Pconvc > 1.05*Max_P/3) {
				Pconvc = 1.05*Max_P/3;
			}
			
			current_A = -(~(complex(Pconva,(Pconva/pf)*sin(acos(pf)))/voltage_A));
			current_B = -(~(complex(Pconvb,(Pconvb/pf)*sin(acos(pf)))/voltage_B));
			current_C = -(~(complex(Pconvc,(Pconvc/pf)*sin(acos(pf)))/voltage_C));

			for (k = 0; k < 6; k++)
			{
				PoutA = Pconva - current_A.Mag()*current_A.Mag()*(AMx[0][0] - AMx[0][1]).Re();
				PoutB = Pconvb - current_B.Mag()*current_B.Mag()*(AMx[1][1] - AMx[0][1]).Re();
				PoutC = Pconvc - current_C.Mag()*current_C.Mag()*(AMx[2][2] - AMx[0][1]).Re();

				QoutA = pf/abs(pf)*PoutA*sin(acos(pf));
				QoutB = pf/abs(pf)*PoutB*sin(acos(pf));
				QoutC = pf/abs(pf)*PoutC*sin(acos(pf));

				current_A = -(~(complex(PoutA,QoutA)/voltage_A));
				current_B = -(~(complex(PoutB,QoutB)/voltage_B));
				current_C = -(~(complex(PoutC,QoutC)/voltage_C));
			}

			EfA = voltage_A - (AMx[0][0] - AMx[0][1])*current_A - AMx[0][2]*(current_A + current_B + current_C);
			EfB = voltage_B - (AMx[1][1] - AMx[1][0])*current_A - AMx[1][2]*(current_A + current_B + current_C);
			EfC = voltage_C - (AMx[2][2] - AMx[2][0])*current_A - AMx[2][1]*(current_A + current_B + current_C);

			
		}
		
		else
			GL_THROW("Unknown generator mode");
	}
	
	//test functions
	double PowerA, PowerB, PowerC, QA, QB, QC;

	PowerA = -voltage_A.Mag()*current_A.Mag()*cos(voltage_A.Arg() - current_A.Arg());
	PowerB = -voltage_B.Mag()*current_B.Mag()*cos(voltage_B.Arg() - current_B.Arg());
	PowerC = -voltage_C.Mag()*current_C.Mag()*cos(voltage_C.Arg() - current_C.Arg());

	QA = -voltage_A.Mag()*current_A.Mag()*sin(voltage_A.Arg() - current_A.Arg());
	QB = -voltage_B.Mag()*current_B.Mag()*sin(voltage_B.Arg() - current_B.Arg());
	QC = -voltage_C.Mag()*current_C.Mag()*sin(voltage_C.Arg() - current_C.Arg());


	TotalRealPow = PowerA + PowerB + PowerC;
	TotalReacPow = QA + QB + QC;

	GenElecEff = TotalRealPow/Pconv * 100;

	Wind_Speed = WSadj;
	
	pLine_I[0] = current_A;
	pLine_I[1] = current_B;
	pLine_I[2] = current_C;
	
	//TIMESTAMP t2 = TS_NEVER;
	
	
	TIMESTAMP t2 = t1 + time_advance;		
	

	
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

complex *windturb_dg::get_complex(OBJECT *obj, char *name)
{
	PROPERTY *p = gl_get_property(obj,name);
	if (p==NULL || p->ptype!=PT_complex)
		return NULL;
	return (complex*)GETADDR(obj,p);
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: windturb_dg
//////////////////////////////////////////////////////////////////////////



EXPORT int create_windturb_dg(OBJECT **obj, OBJECT *parent) 
{
	try 
	{
		*obj = gl_create_object(windturb_dg::oclass);
		if (*obj!=NULL)
		{
			windturb_dg *my = OBJECTDATA(*obj,windturb_dg);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	} 
	catch (const char *msg) 
	{
		gl_error("create_windturb_dg: %s", msg);
		return 0;
	}
	return 0;
}

EXPORT int init_windturb_dg(OBJECT *obj, OBJECT *parent) 
{
	try 
	{
		if (obj!=NULL)
			return OBJECTDATA(obj,windturb_dg)->init(parent);
	}
	catch (const char *msg)
	{
		gl_error("init_windturb_dg(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
		return 0;
	}
	return 0;
}

EXPORT TIMESTAMP sync_windturb_dg(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	TIMESTAMP t2 = TS_NEVER;
	windturb_dg *my = OBJECTDATA(obj,windturb_dg);
	try
	{
		switch (pass) {
		case PC_PRETOPDOWN:
			t2 = my->presync(obj->clock,t1);
			break;
		case PC_BOTTOMUP:
			t2 = my->sync(obj->clock,t1);
			break;
		case PC_POSTTOPDOWN:
		default:
			GL_THROW("invalid pass request (%d)", pass);
			break;
		}
		if (pass==clockpass)
			obj->clock = t1;	

		return t2;
	}
	catch (const char *msg)
	{
		gl_error("sync_windturb_dg(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
		return TS_INVALID;
	}
	catch (...)
	{
		gl_error("%s (wind_turb_dg:%d): %s", obj->name?obj->name:"unnamed", obj->id, "unknown exception");
		return TS_INVALID;
	}
	return TS_INVALID;
}



/*
[1]	Malinga, B., Sneckenberger, J., and Feliachi, A.; "Modeling and Control of a Wind Turbine as a Distributed Resource", 
	Proceedings of the 35th Southeastern Symposium on System Theory, Mar 16-18, 2003, pp. 108-112.

[2]	Cotrell J.R., "A preliminary evaluation of a multiple-generator drivetrain configuration for wind turbines,"
	in Proc. 21st ASME Wind Energy Symposium, 2002, pp. 345-352.
*/