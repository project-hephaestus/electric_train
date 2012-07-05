/** $Id: energy_consuption.cpp,v 1.0 2012/06/28 
	Copyright (C) 2012 University of Thessaly
	@file energy_consuption.cpp

 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <iostream>
#include <fstream>
using namespace std;

#include "generators.h"
#include "energy_consumption.h"
#include "gridlabd.h"

CLASS *energy_consumption::oclass = NULL;
energy_consumption *energy_consumption::defaults = NULL;

static PASSCONFIG passconfig = PC_BOTTOMUP|PC_POSTTOPDOWN;
static PASSCONFIG clockpass = PC_BOTTOMUP;


ofstream myfile;
double *pos_minute; 

//in order to use the object lights
OBJECT *object_lights;
complex *Ener_Lights;

//in order to use the object house
OBJECT *object_ac;
complex *Ener_AC;


/*Overloading operator "<<" in order to write complex numbers in a file*/
ostream &operator <<(ostream &s,complex c)
{
	s<<c.r;

	return s;
}

energy_consumption::energy_consumption(){}


/* Class registration is only called once to register the class with the core */
energy_consumption::energy_consumption(MODULE *module)
{

	
		if (oclass==NULL)
	{
		oclass = gl_register_class(module,"energy_consumption",sizeof(energy_consumption),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s", __FILE__); 


         if (gl_publish_variable(oclass,
			PT_complex, "Energy[Wh]", PADDR(Energy), //the energy of the battery
			PT_int32, "number_of_stations", PADDR(number_of_stations),
		    PT_double, "kind[unit]", PADDR(kind),
			PT_double, "speed", PADDR(speed),
			PT_complex, "power_consumed", PADDR(power_consumed),  //the power that consumed due to the movement of the train
			PT_double, "weight[lb]", PADDR(weight), 
			PT_double, "elevation[unit]", PADDR(elevation),  
            PT_double, "elevation2[unit]", PADDR(elevation2),
			PT_complex_array, "Energy_per", PADDR(Energy_per),  //the energy that the battery has to each station
		
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
		defaults = this;
		memset(this,0,sizeof(energy_consumption));
        
		counter=0;
		power_consumed=1;

		
	}
}

/* Object creation is called once for each object that is created by the core */
int energy_consumption::create2(void) 
{
	memcpy(this,defaults,sizeof(*this));

	
	/* TODO: set the context-free initial value of properties */
	return 1; /* return 1 on success, 0 on failure */
}

/* Object initialization is called once after all object have been created */

int energy_consumption::init2(OBJECT *parent)
{
	OBJECT *obj = OBJECTHDR(this);
	

	//file that contains the distance in terms of klm
	char *inname = "station.txt";
	int z=0;
	int i;
	int ii;
	int passengers=50;
	ofstream myfile2;
	complex remain_energy;
	complex energy_per_kg;


	//pointers of objects that we use in order to take the values of some properties
	object_lights=gl_get_object("installed-power");
	object_ac=gl_get_object("hvac");

    
	//memory allocations
    Energy_per_station = new complex[number_of_stations];	
	pos_minute=new double[number_of_stations];
	pos_station=new double[number_of_stations];


	myfile.open ("file.txt");

	//file that contains the time that the train arrives at each station
	myfile2.open ("stations2.txt");
	
	initial_energy=Energy;
	ifstream infile(inname);

	//read the pos_stations from file
    while (infile >> i) 
	{
		pos_station[z]=i;
		z++;
    }
	
	//take the minutes of arrival of each station and write them in a file  		
	pos_minute[0]=1;

	for(ii=0;ii<number_of_stations;ii++)
	{
		pos_minute[ii+1]=(int)(pos_minute[ii]+((pos_station[ii+1]*1000-pos_station[ii]*1000) /speed)/60);
		myfile2 <<pos_minute[ii]<<endl;
	}
	myfile2.close();
	
	if(kind==0)
	{
		  //lead-acid
		  remain_energy=Energy*0.2;
		  efficiency=0.8;
		  energy_per_kg=0.035;		

	}
    else if(kind==1)
	{
		//Li-ion
		  remain_energy=Energy*0.25;
		  efficiency=0.85;
		  energy_per_kg=0.2;
		  
	}
	else if(kind==2)
    {
		 //Nimh
		  remain_energy=Energy*0.2;
	      efficiency=0.7;
		  energy_per_kg=0.055;
		 
	}
    else throw "Unknown Kind of battery";

	weight=( Energy.r/energy_per_kg.r ) + weight + 60*passengers; //the total weight of the train
	

	return 1; 
}


TIMESTAMP energy_consumption::presync(TIMESTAMP t0, TIMESTAMP t1)
{

	TIMESTAMP t2 = TS_NEVER;
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

TIMESTAMP energy_consumption::sync(TIMESTAMP t0, TIMESTAMP t1) 
{
	    int i;
		double p=1.1843; //air density
		double vwind=4.33; //wind's speed
		double cl=0.15; //drag coefficient;
		double A=10; //frontal area of wagon
		double cr=0.0035; //rolling resistance
		double acceleration;
		
		counter++;
		elevation2=elevation-prev_elevation;
	
		//using the previous pointers in order to acquire the desirable parameters
		Ener_Lights=get_complex(object_lights, "actual_power");
		Ener_AC=get_complex(object_ac, "hvac_power");
		
		//calculation of the acceleration
		for(i=0;i<number_of_stations;i++)
		{
			if(elevation2>0)
			{
				if(counter==1)acceleration = (speed-0)/60;
				else if(counter == pos_minute[i])acceleration = (speed-0)/60;
				else if(counter == pos_minute[i]-1)acceleration = (0-speed)/60;
				else acceleration=0;
			}
			else acceleration=-0.2;
			
		}

		//energy consumption
		if(elevation2>0)
		power_consumed =(*Ener_Lights)+(*Ener_AC)+0.000000278* 60*(speed*0.277)*( 0.5*p*2*cl*A*(speed*0.277+vwind)*(speed*0.277+vwind) +cr*weight*9.82 + weight*(acceleration) + weight * 9.82 *elevation/1000 );
      
	    Energy = Energy -  (power_consumed);
	
	for(i=0;i<number_of_stations;i++)
	{
		if(counter==pos_minute[i])
		{
	
			Energy_per=Energy; 
	        myfile << Energy_per <<"\n";

			break;
		}
	}   
	if(counter== pos_minute[number_of_stations-1] )
	{  
		myfile.close();
	}
	prev_elevation=elevation;
	TIMESTAMP t2 = TS_NEVER;

	return t2; 
}

/* Postsync is called when the clock needs to advance on the second top-down pass */
TIMESTAMP energy_consumption::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	TIMESTAMP t2 = TS_NEVER;
	/* TODO: implement post-topdown behavior */
	

return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

complex *energy_consumption::get_complex(OBJECT *obj, char *name)
{
	PROPERTY *p = gl_get_property(obj,name);
	if (p==NULL || p->ptype!=PT_complex)
		return NULL;
	return (complex*)GETADDR(obj,p);
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int create_energy_consumption(OBJECT **obj, OBJECT *parent) 
{
	try 
	{
		*obj = gl_create_object(energy_consumption::oclass);
		if (*obj!=NULL)
		{
			energy_consumption *my = OBJECTDATA(*obj,energy_consumption);
			gl_set_parent(*obj,parent);
			return my->create2();
		}
	} 
	catch (char *msg) 
	{
		gl_error("create_energy_consumption: %s", msg);
	}
	return 0;
}

EXPORT int init_energy_consumption(OBJECT *obj, OBJECT *parent) 
{
	try 
	{
		if (obj!=NULL)
			return OBJECTDATA(obj,energy_consumption)->init2(parent);
	}
	catch (char *msg)
	{
		gl_error("init_energy_consumption(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
	}
	return 0;
}

EXPORT TIMESTAMP sync_energy_consumption(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	TIMESTAMP t2 = TS_NEVER;
	energy_consumption *my = OBJECTDATA(obj,energy_consumption);
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
		gl_error("sync_energy_consumption(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
	}
	return t2;
}
