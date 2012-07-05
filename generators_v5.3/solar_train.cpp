/** $Id: solar_train.cpp,v 1.0 2012/06/28
	Copyright (C) 2012 University of Thessaly
	@file solar_train.cpp
**/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <complex.h>
#include <math.h>
#include "generators.h"
#include "solar_train.h"
#include "gridlabd.h"

using namespace std;

CLASS *solar_train::oclass = NULL;
solar_train *solar_train::defaults = NULL;

static PASSCONFIG passconfig = PC_BOTTOMUP|PC_POSTTOPDOWN;
static PASSCONFIG clockpass = PC_BOTTOMUP;

complex *test;						//demanding energy
complex *Energy_station;				//energy battery per station
double *station;					//stations of the route
//in order to use the object solar
OBJECT *obj2;
complex *pv_out1;
//in order to use the object solar
OBJECT *obj3;
complex *pv_out2;
//in order to use the object house
OBJECT *obj4;
complex *EnergyLights;
//in order to use the object house
OBJECT *obj5;
complex *EnergyAC;

complex *energy_bet_stations; //the difference of energy between 2 stations
complex *table;				 //in order to determine the distance that we can cover with the remaining energy 	



/* Class registration is only called once to register the class with the core */
solar_train::solar_train(MODULE *module)
{
		if (oclass==NULL)
		{
			oclass = gl_register_class(module,"solar_train",sizeof(solar_train),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN);
			if (oclass==NULL)
				GL_THROW("unable to register object class implemented by %s", __FILE__);


			 if (gl_publish_variable(oclass,
				PT_complex, "Energy1[Wh]", PADDR(Energy1), //the remaining energy of the battery
				PT_complex, "remainenergy[Wh]", PADDR(remainenergy), //the maximum SOD
				PT_complex, "energy_between_stations2[Wh]", PADDR(energy_between_stations2),
				PT_complex, "energy_between_stations1[Wh]", PADDR(energy_between_stations1),
				PT_double, "kind", PADDR(kind), //defines the kind of the battery (0:Lead-acid, 1:Li-ion, 2:NiMH)
				PT_double, "area[sf]", PADDR(area), //the area of the roof of the train 
				PT_double, "speed[mps]", PADDR(speed), //the speed of the train
				PT_double, "elevation[unit]", PADDR(elevation),
				PT_double, "power_consumed[Wh]", PADDR(power_consumed),	//the nessecary for the movement of the train
				PT_complex, "passengers[unit]", PADDR(passengers),
				PT_double, "efficiency[unit]", PADDR(efficiency), //efficiency of battery
				PT_double, "weight[lb]", PADDR(weight), //train's weight
				PT_double, "prev_elevation[Wh]", PADDR(prev_elevation),
				PT_int32, "num_stations", PADDR(num_stations),
				PT_complex_array, "e_per_station", PADDR(e_per_station), //defines the level of charge of the battery
					NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
			defaults = this;
			memset(this,0,sizeof(solar_train));

			//Initialization
			change=0;                   		//flag that defines when the battery must be changed
			counter=0;							//defines the number of the iteration
			station1=0;							//in order to indicate the location of the train
			station2=0;							//in order to indicate the location of the train
			weight=0;							
			passengers=0.0;
			plug_in=0;							//flag that defines when to shift from
			efficiency=0.05;					
			kind=-1;
			current_station=-1;					//flag that defines the station in which the train is
		    prev_elevation=10.50;				//it is used to take the difference between the current elevation and the previous one
		}
}

/* Object creation is called once for each object that is created by the core */
int solar_train::create2(void)
{
	memcpy(this,defaults,sizeof(*this));
	/* TODO: set the context-free initial value of properties */
	return 1; /* return 1 on success, 0 on failure */
}

/* Object initialization is called once after all object have been created */
int solar_train::init2(OBJECT *parent)
{
	OBJECT *obj = OBJECTHDR(this);
	int c,k=0;
	double i;
    char *inname = "file.txt";

    par=parent; //define the object's parent

	//memory allocations
    e_per_station=new complex[num_stations+2];
	test=new complex[num_stations+1];
	Energy_station=new complex[num_stations];
	station=new double[num_stations];
	energy_bet_stations=new complex[num_stations-1];
    table=new complex[num_stations-2];

	initial_energy = Energy1;
	
	ifstream infile(inname);

	while (infile >> i)
	{
		e_per_station[k]=i;
		k++;
    }
   
	for(k=0;k<num_stations-1;k++)
	{
		energy_bet_stations[k]=e_per_station[k]-e_per_station[k+1];	
		
	}
    


 	if(kind==0)
	{
		  //lead-acid
		  remainenergy=Energy1*0.2;
		  efficiency=0.8;
		  energyperkg=0.035;
	}
    else if(kind==1)
	{
		//Li-ion
		  remainenergy=Energy1*0.25;
		  efficiency=0.85;
		  energyperkg=0.2;
	}
	else if(kind==2)
    {
		 //Nimh
		  remainenergy=Energy1*0.2;
		  efficiency=0.7;
		  energyperkg=0.055;
	}
    else throw "Unknown Kind of battery";

	power_consumed=0.1*speed;
	weight=Energy1.r/energyperkg.r +weight+60*passengers; //the total weight of the train
	
	for(c=0;c<num_stations;c++)
	{
		Energy_station[c]=remainenergy;
		station[c]=0;
	}

    //read stations from a file
    int z=0;
	double x;
	char *inname2 = "stations2.txt";

    ifstream infile2(inname2);

    while (infile2 >> x)
	{
		station[z]=(int)x;
		//cout << station[z]<<endl;
		z++;
    }

    //pointers of objects that we use in order to take the values of some properties
	obj2=gl_get_object("sol");
	obj3=gl_get_object("sol2");
	obj4=gl_get_object("installed-power");
	obj5=gl_get_object("hvac");

	if (Energy1==0)throw "Wrong";

	return 1;
}


TIMESTAMP solar_train::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	TIMESTAMP t2 = TS_NEVER;
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

TIMESTAMP solar_train::sync(TIMESTAMP t0, TIMESTAMP t1)
{
		
	    double *radi;						 //radiation
		double p=1.1843;					 //air density
		double vwind=4.33;					 //wind's speed
		double cl=0.15;						 //drag coefficient;
		double A=10;						 //frontal area of wagon
		double cr=0.0035;					 //rolling resistance
		double acceleration;				 //the acceleration of the train(positive:acceleration,negative:deceleration)
		complex  electric_energy_per_minute1; //the amount of energy that is gained by the pv panels at the stations
		int i,j,k,c;
		int xx=0;
		complex error;						  //in order to avoid the overflow of battery 

		//using the previous pointers in order to acquire the desirable parameters
		pv_out1=get_complex(obj2, "VA_Out");
		pv_out2=get_complex(obj3, "VA_Out");
		EnergyLights=get_complex(obj4, "actual_power");
		EnergyAC=get_complex(obj5, "hvac_power");
		radi=gl_get_double(par, gl_get_property(par,"solar_flux"));
		error=initial_energy*0.08;
		counter++;

		//reinitialization in order not to keep the old values
       	for(c=0;c<num_stations+1;c++)
       				test[c]=0;
											
		elevation2=elevation-prev_elevation; //defines the elevation difference between the iterations
		
		//calculation of the acceleration
		for(i=0;i<num_stations;i++)
		{
			if(elevation2>0)
			{
				if(counter==1)acceleration = (speed-0)/60;
				else if(counter == station[i])acceleration = (speed-0)/60;
				else if(counter == station[i]-1)acceleration = (0-speed)/60;
				else acceleration=0;
			}
			else acceleration=-0.2;
		
		}
	
		//energy consumption
		if(elevation2>0)
			 power_consumed =(*EnergyAC)+(*EnergyLights)+0.000000278* 60*(speed*0.277)*( 0.5*p*2*cl*A*(speed*0.277+vwind)*(speed*0.277+vwind) +cr*weight*9.82 + weight*(acceleration) + weight * 9.82 *elevation/1000 );

		//energy that pv panels provide
		electric_energy_per_minute1=(*pv_out1/1000)*60;	//pv_out1
		electric_energy_per_minute=(*pv_out2/1000)*60;	//pv_out2
      
		//Discharging of train's battery
		Energy1 = Energy1 - ( power_consumed)*(1/efficiency);//+electric_energy_per_minute*(1 /efficiency);

		//determines,according to the location of the train, a forecast for the distance that the train can cover
    	for(i=0;i<num_stations-1; i++)
		{
				if(counter < station[i])
				{
					for(j=i+1;j<num_stations;j++)
					{
						  test[j]=test[j-1]+energy_bet_stations[j-1];
						  table[xx]=test[j];
						  xx++;

					}
					
					//check_energy=Energy_station[i];
					current_station = i;
					station1=station[i-1];
					station2=station[i];

					energy_between_stations1=(e_per_station[i-1]-e_per_station[i]);

					if(i== num_stations-1)energy_between_stations2=0;
					else energy_between_stations2=(e_per_station[i]-e_per_station[i+1]);
					
					break;
				}
		}
 
		//Charging of batteries in each station
		for(i=0;i<num_stations;i++)
		{
			if(i!=current_station)
			{
				if(Energy_station[i] + error <  initial_energy.r)
				{
					Energy_station[i] += electric_energy_per_minute*efficiency; 
				}
			}
		}
		ektimisi=Energy_station[current_station] + electric_energy_per_minute*(station1-counter);
		
		//determines when to shift from charging by pv panels to charging by power plants
	    if(counter < station2 && counter > station1)
	    {
			
			if(Energy1 -energy_between_stations2.r < remainenergy.r ){change=1;plug_in=1;}
			else if(Energy1 > (energy_between_stations1.r/(station2-counter)) + energy_between_stations2.r )
			{
				change=0;
			}
			else
			{
				k=-1;
				change=1;

				if(station1==station[0]&&station2==station[1])
					if(ektimisi > test[k+4] && ektimisi < test[k+5] && test[k+5] < initial_energy*0.25)plug_in=1;

			
				if(ektimisi < test[k+1] )plug_in=1;
				else if(ektimisi > test[k+1] && ektimisi < test[k+2] && test[k+2] < initial_energy*0.25)plug_in=1;
				else if(ektimisi > test[k+2] && ektimisi < test[k+3] && test[k+3] < initial_energy*0.25)plug_in=1;
				else if(ektimisi > test[k+3] && ektimisi < test[k+3] && test[k+4] < initial_energy*0.25)plug_in=1;
			}
		}

		if(plug_in==1) //charging rate accomplished by power plants
		{	
			if(Energy_station[current_station] + error <  initial_energy.r)
			     Energy_station[current_station] += electric_energy_per_minute*1.25;
				
		
		}
		//defines when the battery must be changed
		if(change==1)
		{

			for(i=0;i<num_stations;i++)
			{

		      	if(counter == station[i])
		      	{
					change=0;
					cout <<"Stash" <<station[i]<<endl;
					Energy1=Energy_station[current_station];
				}
			}
		}

		prev_elevation=elevation;

		TIMESTAMP t2 = TS_NEVER;

		return t2;
}

/* Postsync is called when the clock needs to advance on the second top-down pass */
TIMESTAMP solar_train::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	TIMESTAMP t2 = TS_NEVER;
	/* TODO: implement post-topdown behavior */
return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}
complex *solar_train::get_complex(OBJECT *obj, char *name)
{
	PROPERTY *p = gl_get_property(obj,name);
	if (p==NULL || p->ptype!=PT_complex)
		return NULL;
	return (complex*)GETADDR(obj,p);
}
//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////
EXPORT int create_solar_train(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(solar_train::oclass);
		if (*obj!=NULL)
		{
			solar_train *my = OBJECTDATA(*obj,solar_train);
			gl_set_parent(*obj,parent);
			return my->create2();
		}
	}
	catch (char *msg)
	{
		gl_error("create_solar_train: %s", msg);
	}
	return 0;
}

EXPORT int init_solar_train(OBJECT *obj, OBJECT *parent)
{
	try
	{
		if (obj!=NULL)
			return OBJECTDATA(obj,solar_train)->init2(parent);
	}
	catch (char *msg)
	{
		gl_error("init_solar_train(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
	}
	return 0;
}

EXPORT TIMESTAMP sync_solar_train(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	TIMESTAMP t2 = TS_NEVER;
	solar_train *my = OBJECTDATA(obj,solar_train);
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
			t2 = my->postsync(obj->clock,t1);
			break;
		default:
			GL_THROW("invalid pass request (%d)", pass);
			break;
		}
		if (pass==clockpass)
			obj->clock = t1;
	}
	catch (char *msg)
	{
		gl_error("sync_solar_train(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
	}
	return t2;
}
