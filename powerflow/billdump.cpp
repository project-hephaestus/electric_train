#include "billdump.h"

// $Id: billdump.cpp 1182 2008-12-22 22:08:36Z dchassin $
/**	Copyright (C) 2008 Battelle Memorial Institute

	@file billdump.cpp

	@{
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "billdump.h"
#include "node.h"

//////////////////////////////////////////////////////////////////////////
// billdump CLASS FUNCTIONS
//////////////////////////////////////////////////////////////////////////

CLASS* billdump::oclass = NULL;

billdump::billdump(MODULE *mod)
{
	if (oclass==NULL)
	{
		// register the class definition
		oclass = gl_register_class(mod,"billdump",sizeof(billdump),PC_BOTTOMUP);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s",__FILE__);

		// publish the class properties
		if (gl_publish_variable(oclass,
			PT_char32,"group",PADDR(group),PT_DESCRIPTION,"the group ID to output data for (all nodes if empty)",
			PT_timestamp,"runtime",PADDR(runtime),PT_DESCRIPTION,"the time to check voltage data",
			PT_char256,"filename",PADDR(filename),PT_DESCRIPTION,"the file to dump the voltage data into",
			PT_int32,"runcount",PADDR(runcount),PT_ACCESS,PA_REFERENCE,PT_DESCRIPTION,"the number of times the file has been written to",
			PT_enumeration,"meter_type",PADDR(meter_type), PT_DESCRIPTION, "describes whether to collect from 3-phase or S-phase meters",
				PT_KEYWORD,"TRIPLEX_METER",METER_TP,
				PT_KEYWORD,"METER",METER_3P,
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
		
	}
}


int billdump::create(void)
{
	memset(group, 0, sizeof(char32));
	runtime = TS_NEVER;
	runcount = 0;
	meter_type = METER_TP;
	return 1;
}

int billdump::init(OBJECT *parent)
{
	return 1;
}

int billdump::isa(char *classname)
{
	return strcmp(classname,"billdump")==0;
}

void billdump::dump(TIMESTAMP t){
	char namestr[64];
	char timestr[64];
	FINDLIST *nodes = NULL;
	OBJECT *obj = NULL;
	FILE *outfile = NULL;
	triplex_meter *pnode;
	meter *qnode;
	

//	CLASS *nodeclass = NULL;
//	PROPERTY *vA, *vB, *vC;

	if (meter_type == METER_TP)
	{
		if(group[0] == 0){
			nodes = gl_find_objects(FL_NEW,FT_CLASS,SAME,"triplex_meter",FT_END);
		} else {
			nodes = gl_find_objects(FL_NEW,FT_CLASS,SAME,"triplex_meter",AND,FT_GROUPID,SAME,group,FT_END);
		}
	}
	else
	{
		if(group[0] == 0){
			nodes = gl_find_objects(FL_NEW,FT_CLASS,SAME,"meter",FT_END);
		} else {
			nodes = gl_find_objects(FL_NEW,FT_CLASS,SAME,"meter",AND,FT_GROUPID,SAME,group,FT_END);
		}
	}

	if(nodes == NULL){
		gl_warning("no nodes were found to dump");
		return;
	}

	outfile = fopen(filename, "w");
	if(outfile == NULL){
		gl_error("billdump unable to open %s for output", filename);
		return;
	}

	//nodeclass = node::oclass;
	//vA=gl_find_property(nodeclass, "

	if (meter_type == METER_TP)
	{
		/* print column names */
		gl_printtime(t, timestr, 64);
		fprintf(outfile,"# %s run at %s on %i triplex meters\n", filename, timestr, nodes->hit_count);
		fprintf(outfile,"meter_name,previous_monthly_bill,previous_monthly_energy\n");
		while (obj=gl_find_next(nodes,obj)){
			if(gl_object_isa(obj, "triplex_meter", "powerflow")){
				pnode = OBJECTDATA(obj,triplex_meter);
				if(obj->name == NULL){
					sprintf(namestr, "%s:%i", obj->oclass->name, obj->id);
				}
				fprintf(outfile,"%s,%f,%f\n",(obj->name ? obj->name : namestr),pnode->previous_monthly_bill,pnode->previous_monthly_energy);
			}
		}
	}
	else
	{
		/* print column names */
		gl_printtime(t, timestr, 64);
		fprintf(outfile,"# %s run at %s on %i meters\n", filename, timestr, nodes->hit_count);
		fprintf(outfile,"meter_name,previous_monthly_bill,previous_monthly_energy\n");
		while (obj=gl_find_next(nodes,obj)){
			if(gl_object_isa(obj, "meter", "powerflow")){
				qnode = OBJECTDATA(obj,meter);
				if(obj->name == NULL){
					sprintf(namestr, "%s:%i", obj->oclass->name, obj->id);
				}
				fprintf(outfile,"%s,%f,%f\n",(obj->name ? obj->name : namestr),qnode->previous_monthly_bill,qnode->previous_monthly_energy);
			}
		}
	}
	fclose(outfile);
}

int billdump::commit(TIMESTAMP t){
	if(runtime == 0){
		runtime = t;
	}
	if((t == runtime || runtime == TS_NEVER) && (runcount < 1)){
		/* dump */
		dump(t);
		++runcount;
	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: billdump
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_billdump(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(billdump::oclass);
		if (*obj!=NULL)
		{
			billdump *my = OBJECTDATA(*obj,billdump);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (const char *msg)
	{
		gl_error("create_billdump: %s", msg);
	}
	return 0;
}

EXPORT int init_billdump(OBJECT *obj)
{
	billdump *my = OBJECTDATA(obj,billdump);
	try {
		return my->init(obj->parent);
	}
	catch (const char *msg)
	{
		gl_error("%s (billdump:%d): %s", obj->name, obj->id, msg);
		return 0; 
	}
}

EXPORT TIMESTAMP sync_billdump(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	billdump *my = OBJECTDATA(obj,billdump);
	TIMESTAMP rv;
	obj->clock = t1;
	rv = my->runtime > t1 ? my->runtime : TS_NEVER;
	return rv;
}

EXPORT int commit_billdump(OBJECT *obj){
	billdump *my = OBJECTDATA(obj,billdump);
	try {
		return my->commit(obj->clock);
	} catch(char *msg){
		gl_error("%s (billdump:%d): %s", obj->name, obj->id, msg);
		return 0; 
	}
}

EXPORT int isa_billdump(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,billdump)->isa(classname);
}

/**@}*/


// EOF
