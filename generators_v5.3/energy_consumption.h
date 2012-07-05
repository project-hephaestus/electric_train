/** $Id: energy_consumption.h,v 1.0 2012/06/28 
	Copyright (C) 2012 University of Thessaly
	@file energy_consumption.h

 **/


#ifndef _energy_consumption_H
#define _energy_consumption_H

#include <stdarg.h>
#include "../powerflow/powerflow_object.h"
//#include "../powerflow/node.h"
#include "gridlabd.h"

class energy_consumption
{
private:
	/* TODO: put private variables here */
	//complex AMx[3][3];//generator impedance matrix

	
	
protected:
	/* TODO: put unpublished but inherited variables */
public:
	/* TODO: put published variables here */
	TIMESTAMP prev_time;
	int flag;
	
	int number_of_stations;
	complex Energy;
	double *pos_station;
	complex *Energy_per_station;
	complex Energy_per;
	int counter;
	double elevation2;
	double elevation;
	double prev_elevation;
	complex power_consumed;
	double speed;
	double weight;
	complex prev_energy;
	complex initial_energy;
    double kind;
    double efficiency;

	
	

	

public:
	/* required implementations */
	energy_consumption();	
	energy_consumption(MODULE *module);
	int create2(void);
	int init2(OBJECT *parent);
	

	TIMESTAMP presync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP postsync(TIMESTAMP t0, TIMESTAMP t1);

	complex *get_complex(OBJECT *obj, char *name);
public:
	static CLASS *oclass;
	static energy_consumption *defaults;
#ifdef OPTIONAL
	static CLASS *pclass; /**< defines the parent class */
	TIMESTAMPP plc(TIMESTAMP t0, TIMESTAMP t1); /**< defines the default PLC code */
#endif
};

#endif

/**@}*/
/** $Id: solar.h,v 1.0 2008/07/18
	@file solar.h
	@addtogroup solar
	@ingroup MODULENAME

 @{  
 **/

#ifndef _energy_consumption_H
#define _energy_consumption_H

#include <stdarg.h>
#include "gridlabd.h"

class energy_consumption {
private:
	/* TODO: put private variables here */
protected:
	/* TODO: put unpublished but inherited variables */
public:
	/* TODO: put published variables here */
public:
	/* required implementations */
	energy_consumption(void);
	energy_consumption(MODULE *module);
	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP presync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP postsync(TIMESTAMP t0, TIMESTAMP t1);
public:
	static CLASS *oclass;
	static energy_consumption *defaults;
#ifdef OPTIONAL
	static CLASS *pclass; /**< defines the parent class */
	TIMESTAMPP plc(TIMESTAMP t0, TIMESTAMP t1); /**< defines the default PLC code */
#endif
};

#endif

/**@}*/
