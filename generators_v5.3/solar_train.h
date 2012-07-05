/** $Id: solar_train.h,v 1.0 2012/06/28 
	Copyright (C) 2012 University of Thessaly
	@file solar_train.h

 **/

#ifndef _solar_train_H
#define _solar_train_H

#include <stdarg.h>
#include "../powerflow/powerflow_object.h"
#include "../powerflow/node.h"
#include "gridlabd.h"
#include "energy_consumption.h"

class solar_train
{
private:

protected:
	
public:
	/* put published variables here */
	TIMESTAMP prev_time;
	int first_time_step;
	
    object par;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
	complex remainenergy;  
	complex initial_energy;
	complex energy_between_stations2;
	complex energy_between_stations1;
	complex energyperkg;					//the amount of energy in a battery per kg
	complex power_consumed;
	complex Energy1;
	complex electric_energy_per_minute;		//the amount of energy that is gained by the pv panels on the roof
	complex prev_energy;
	complex ektimisi;						//forecast for the level of charge
	complex *e_per_station;
	double kind;
	double efficiency;
   	double radiation;
	double elevation;
	double elevation2;
	double prev_elevation;
	double speed;
	double weight;
	double passengers;
	double area;
	double counter;
	double station1;
	double station2;
	double plug_in;
	double change;
	int lights;
	int ac;
	int current_station;
	int num_stations;
	
	
	

public:
	/* required implementations */
	solar_train(MODULE *module);
	int create2(void);
	int init2(OBJECT *parent);
	
	TIMESTAMP presync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP postsync(TIMESTAMP t0, TIMESTAMP t1);
	
	complex *get_complex(OBJECT *obj, char *name);
public:
	static CLASS *oclass;
	static solar_train *defaults;
#ifdef OPTIONAL
	static CLASS *pclass; /**< defines the parent class */
	TIMESTAMPP plc(TIMESTAMP t0, TIMESTAMP t1); /**< defines the default PLC code */
#endif
};

#endif

/** $Id: solar_train.h,v 1.0 2012/06/28 
	Copyright (C) 2012 University of Thessaly
	@file solar_train.cpp

 **/

#ifndef _solar_train_H
#define _solar_train_H

#include <stdarg.h>
#include "gridlabd.h"

class solar_train {
private:
	/* TODO: put private variables here */
protected:
	/* TODO: put unpublished but inherited variables */
public:
	/* TODO: put published variables here */
public:
	/* required implementations */
	solar_train(MODULE *module);
	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP presync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP postsync(TIMESTAMP t0, TIMESTAMP t1);
public:
	static CLASS *oclass;
	static solar_train *defaults;
#ifdef OPTIONAL
	static CLASS *pclass; /**< defines the parent class */
	TIMESTAMPP plc(TIMESTAMP t0, TIMESTAMP t1); /**< defines the default PLC code */
#endif
};

#endif

/**@}*/
