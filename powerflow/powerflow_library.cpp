/** $Id: powerflow_library.cpp 858 2008-08-28 21:06:13Z d3g637 $
	Copyright (C) 2008 Battelle Memorial Institute
	@file powerflow_library.cpp
	@addtogroup powerflow_library Powerflow library (abstract)
	@ingroup powerflow

	The powerflow_library class is an abstract class that implements 
	basic elements of powerflow object configurations.  

 @{
 **/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "powerflow.h"
#include "powerflow_library.h"

//////////////////////////////////////////////////////////////////////////
// powerflow_library CLASS FUNCTIONS
//////////////////////////////////////////////////////////////////////////
CLASS* powerflow_library::oclass = NULL;
CLASS* powerflow_library::pclass = NULL;

powerflow_library::powerflow_library(MODULE *mod)
{	
	if (oclass==NULL)
	{
		oclass = gl_register_class(mod,"powerflow_library",sizeof(powerflow_library),PC_NOSYNC);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s",__FILE__);
	}
}

powerflow_library::powerflow_library(CLASS *oclass)
{
	gl_create_foreign((OBJECT*)this);
}

int powerflow_library::isa(char *classname)
{
	return strcmp(classname,"powerflow_library")==0;
}

int powerflow_library::create(void)
{
	return 1;
}

int powerflow_library::init(OBJECT *parent)
{
	return 1;
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: powerflow_library
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_powerflow_library(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(powerflow_library::oclass);
		if (*obj!=NULL)
		{
			powerflow_library *my = OBJECTDATA(*obj,powerflow_library);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (const char *msg)
	{
		gl_error("create_powerflow_library: %s", msg);
	}
	return 0;
}

/**
* Object initialization is called once after all object have been created
*
* @param obj a pointer to this object
* @return 1 on success, 0 on error
*/
EXPORT int init_powerflow_library(OBJECT *obj)
{
	powerflow_library *my = OBJECTDATA(obj,powerflow_library);
	try {
		return my->init(obj->parent);
	}
	catch (const char *msg)
	{
		gl_error("%s (powerflow_library:%d): %s", my->get_name(), my->get_id(), msg);
		return 0; 
	}
}

/**
* Sync is called when the clock needs to advance on the bottom-up pass (PC_BOTTOMUP)
*
* @param obj the object we are sync'ing
* @param t0 this objects current timestamp
* @param pass the current pass for this sync call
* @return t1, where t1>t0 on success, t1=t0 for retry, t1<t0 on failure
*/
EXPORT TIMESTAMP sync_powerflow_library(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	powerflow_library *pObj = OBJECTDATA(obj,powerflow_library);
	try {
		switch (pass) {
		case PC_PRETOPDOWN:
		case PC_BOTTOMUP:
		case PC_POSTTOPDOWN:
		default:
			throw "invalid pass request";
		}
	} catch (const char *error) {
		gl_error("%s (powerflow_library:%d): %s", pObj->get_name(), pObj->get_id(), error);
		return TS_INVALID; 
	} catch (...) {
		gl_error("%s (powerflow_library:%d): %s", pObj->get_name(), pObj->get_id(), "unknown exception");
		return TS_INVALID;
	}
}

EXPORT int isa_powerflow_library(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,powerflow_library)->isa(classname);
}

/**@}**/
