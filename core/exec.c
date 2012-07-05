/** $Id: exec.c 1188 2009-01-02 21:51:07Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file exec.c
	@addtogroup exec Main execution loop
	@ingroup core
	
	The main execution loop sets up the main simulation, initializes the
	objects, and runs the simulation until it either settles to equilibrium
	or runs into a problem.  It also takes care multicore/multiprocessor
	parallelism when possible.  Objects of the same rank will be synchronized
	simultaneously, resources permitting.

	The main processing loop calls each object passing to it a TIMESTAMP
	indicating the desired synchronization time.  The sync() call attempts to
	advance the object's internal clock to the time indicated, and if successful it
	returns the time of the next expected change in the object's state.  An
	object state change is one which requires the equilibrium equations of
	the object to be updated.  When an object's state changes, all the other
	objects in the simulator are given an opportunity to consider the change
	and possibly alter the time of their next state change.  The core
	continues calling objects, advancing the global clock when
	necessary, and continuing in this way until all objects indicate that
	no further state changes are expected.  This is the equilibrium condition
	and the simulation consequently ends.

	@future [Chassin Oct'07]

	There is some value in exploring whether it is necessary to update all
	objects when a particular objects implements a state change.  The idea is
	based on the fact that updates propagate through the model based on known
	relations, such at the parent-child relation or the link-node relation.
	Consequently, it should obvious that unless a value in a related object
	has changed, there can be no significant change to an object that hasn't reached
	it's declared update time.  Thus only the object that "won" the next update
	time and those that are immediately related to it need be updated.  This 
	change could result in a very significant improvement in performance,
	particularly in models with many lightly coupled objects. 

 @{
 **/

#include <signal.h>
#include <ctype.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#include <winbase.h>
#include <direct.h>
#include <sys/timeb.h>
#else
#include <unistd.h>
#endif
#include "output.h"
#include "exec.h"
#include "class.h"
#include "convert.h"
#include "object.h"
#include "index.h"
#include "realtime.h"
#include "module.h"
#include "threadpool.h"
#include "debug.h"
#include "exception.h"
#include "random.h"	
#include "local.h"
#include "schedule.h"
#include "loadshape.h"
#include "enduse.h"
#include "globals.h"
#include "math.h"
#include "time.h"
#include "lock.h"

#include "pthread.h"

/** The main system initialization sequence
	@return 1 on success, 0 on failure
 **/

int exec_init()
{
#if 0
#ifdef WIN32
	char glpathvar[1024];
#endif
#endif

	size_t glpathlen=0;
	/* setup clocks */
	global_starttime = realtime_now();
	timestamp_set_tz(NULL);

	/* determine current working directory */
	getcwd(global_workdir,1024);

	/* save locale for simulation */
	locale_push();

#if 0 /* isn't cooperating for strange reasons -mh */
#ifdef WIN32
	glpathlen=strlen("GLPATH=");
	sprintf(glpathvar, "GLPATH=");
	ExpandEnvironmentStrings(getenv("GLPATH"), glpathvar+glpathlen, (DWORD)(1024-glpathlen));
#endif
#endif

	if (global_init()==FAILED)
		return 0;
	return 1;
}

//sjin: GetMachineCycleCount
/*int mc_start_time;
int mc_end_time;
int GetMachineCycleCount(void)
{      
   __int64 cycles;
   _asm rdtsc; // won't work on 486 or below - only pentium or above

   _asm lea ebx,cycles;
   _asm mov [ebx],eax;
   _asm mov [ebx+4],edx;
   return cycles;
}*/
clock_t start, end;

#ifndef _MAX_PATH
#define _MAX_PATH 1024
#endif

#define PASSINIT(p) (p % 2 ? ranks[p]->first_used : ranks[p]->last_used)
#define PASSCMP(i, p) (p % 2 ? i <= ranks[p]->last_used : i >= ranks[p]->first_used)
#define PASSINC(p) (p % 2 ? 1 : -1)

static struct thread_data *thread_data = NULL;
static INDEX **ranks = NULL;
const PASSCONFIG passtype[] = {PC_PRETOPDOWN, PC_BOTTOMUP, PC_POSTTOPDOWN};
static unsigned int pass;
int iteration_counter = 0;   /* number of redos completed */

#ifndef NOLOCKS
int64 lock_count = 0, lock_spin = 0;
#endif

extern int stop_now;

//sjin: struct for pthread_create arguments
struct arg_data {
	int thread;
	void *item;
	int incr;
};

INDEX **exec_getranks(void)
{
	return ranks;
}

static STATUS setup_ranks(void)
{
	OBJECT *obj;
	int i;
	static INDEX *passlist[] = {NULL,NULL,NULL,NULL}; /* extra NULL marks the end of the list */

	/* create index object */
	ranks = passlist;
	ranks[0] = index_create(0,10);
	ranks[1] = index_create(0,10);
	ranks[2] = index_create(0,10);

	/* build the ranks of each pass type */
	for (i=0; i<sizeof(passtype)/sizeof(passtype[0]); i++)
	{
		if (ranks[i]==NULL)
			return FAILED;

		/* add every object to index based on rank */
		for (obj=object_get_first(); obj!=NULL; obj=object_get_next(obj))
		{
			/* ignore objects that don't use this passconfig */
			if ((obj->oclass->passconfig&passtype[i])==0)
				continue;

			/* add this object to the ranks for this passconfig */
			if (index_insert(ranks[i],obj,obj->rank)==FAILED) 
				return FAILED;
			//sjin: print out obj id, pass, rank information
			//else 
			//	printf("obj[%d]: pass = %d, rank = %d\n", obj->id, passtype[i], obj->rank);
		}

		if (global_debug_mode==0 && global_nolocks==0)

			/* shuffle the objects in the index */
			index_shuffle(ranks[i]);
	}

	return SUCCESS;
}

char *simtime(void)
{
	static char buffer[64];
	return convert_from_timestamp(global_clock,buffer,sizeof(buffer))>0?buffer:"(invalid)";
}

static STATUS show_progress(void)
{
	output_progress();
	/* reschedule report */
	realtime_schedule_event(realtime_now()+1,show_progress);
	return SUCCESS;
}

//sjin: implement new ss_do_object_sync for pthreads
static void ss_do_object_sync(int thread, void *item)
{
	struct sync_data *data = &thread_data->data[thread];
	OBJECT *obj = (OBJECT *) item;
	TIMESTAMP this_t;
	
	//printf("thread %d\t%d\t%s\n", thread, obj->rank, obj->name);
	//this_t = object_sync(obj, global_clock, passtype[pass]);

	/* check in and out-of-service dates */
	if (global_clock<obj->in_svc)
		this_t = obj->in_svc; /* yet to go in service */
	else if (global_clock<=obj->out_svc)
	{
		this_t = object_sync(obj, global_clock, passtype[pass]);
#ifdef _DEBUG
		/* sync dumpfile */
		if (global_sync_dumpfile[0]!='\0')
		{
			static FILE *fp = NULL;
			if (fp==NULL)
			{
				static int tried = 0;
				if (!tried)
				{
					fp = fopen(global_sync_dumpfile,"wt");
					if (fp==NULL)
						output_error("sync_dumpfile '%s' is not writeable", global_sync_dumpfile);
					else
						fprintf(fp,"timestamp,pass,iteration,thread,object,sync\n");	
					tried = 1;
				}
			}
			if (fp!=NULL)
			{
				static int64 lasttime = 0;
				static char lastdate[64]="";
				char syncdate[64]="";
				static char *passname;
				static int lastpass = -1;
				char objname[1024];
				if (lastpass!=passtype[pass])
				{
					lastpass=passtype[pass];
					switch(lastpass) {
					case PC_PRETOPDOWN: passname = "PRESYNC"; break;
					case PC_BOTTOMUP: passname = "SYNC"; break;
					case PC_POSTTOPDOWN: passname = "POSTSYNC"; break;
					default: passname = "UNKNOWN"; break;
					}
				}
				if (lasttime!=global_clock)
				{
					lasttime = global_clock;
					convert_from_timestamp(global_clock,lastdate,sizeof(lastdate));
				}
				convert_from_timestamp(this_t<0?-this_t:this_t,syncdate,sizeof(syncdate));
				if (obj->name==NULL) sprintf(objname,"%s:%d", obj->oclass->name, obj->id);
				else strcpy(objname,obj->name);
				fprintf(fp,"%s,%s,%d,%d,%s,%s\n",lastdate,passname,global_iteration_limit-iteration_counter,thread,objname,syncdate);
			}
		}
#endif
	}
	else 
		this_t = TS_NEVER; /* already out of service */

	/* check for "soft" event (events that are ignored when stopping) */
	if (this_t < -1)
		this_t = -this_t;
	else if (this_t != TS_NEVER)
		data->hard_event++;  /* this counts the number of hard events */

	/* check for stopped clock */
	if (this_t < global_clock) {
		output_error("%s: object %s stopped its clock (exec)!", simtime(), object_name(obj));
		/* TROUBLESHOOT
			This indicates that one of the objects in the simulator has encountered a
			state where it cannot calculate the time to the next state.  This usually
			is caused by a bug in the module that implements that object's class.
		 */
		data->status = FAILED;
	} else {
		/* check for iteration limit approach */
		if (iteration_counter == 2 && this_t == global_clock) {
			output_verbose("%s: object %s iteration limit imminent",
								simtime(), object_name(obj));
		}
		else if (iteration_counter == 1 && this_t == global_clock) {
			output_error("convergence iteration limit reached for object %s:%d", obj->oclass->name, obj->id);
			/* TROUBLESHOOT
				This indicates that the core's solver was unable to determine
				a steady state for all objects for any time horizon.  Identify
				the object that is causing the convergence problem and contact
				the developer of the module that implements that object's class.
			 */
		}

		/* manage minimum timestep */
		if (global_minimum_timestep>1 && this_t>global_clock && this_t<TS_NEVER)
			this_t = (((this_t-1)/global_minimum_timestep)+1)*global_minimum_timestep;

		/* if this event precedes next step, next step is now this event */
		if (data->step_to > this_t) {
			//LOCK(data);
			data->step_to = this_t;
			//UNLOCK(data);
		}
		//printf("data->step_to=%d, this_t=%d\n", data->step_to, this_t);
	}
}

//sjin: implement new ss_do_object_sync_list for pthreads
static void *ss_do_object_sync_list(void *threadarg)
{
	LISTITEM *ptr;
	int iPtr;

	struct arg_data *mydata = (struct arg_data *) threadarg;
	int thread = mydata->thread;
	void *item = mydata->item;
	int incr = mydata->incr;

	iPtr = 0;
	for (ptr = item; ptr != NULL; ptr=ptr->next) {
		if (iPtr < incr) {
			ss_do_object_sync(thread, ptr->data);
			iPtr++;
		}
	}
	return NULL;
}

static STATUS init_all(void)
{
	OBJECT *obj;
	STATUS rv = SUCCESS;
	output_verbose("initializing objects...");

	/* initialize loadshapes */
	if (loadshape_initall()==FAILED || enduse_initall()==FAILED)
		return FAILED;

	TRY {
		for (obj=object_get_first(); obj!=NULL; obj=object_get_next(obj))
		{
			if (object_init(obj)==FAILED){
				THROW("init_all(): object %s initialization failed", object_name(obj));
				/* TROUBLESHOOT
					The initialization of the named object has failed.  Make sure that the object's
					requirements for initialization are satisfied and try again.
				 */
			}
			if((obj->oclass->passconfig & PC_FORCE_NAME) == PC_FORCE_NAME){
				if(0 == strcmp(obj->name, "")){
					output_warning("init: object %s:%d should have a name, but doesn't", obj->oclass->name, obj->id);
					/* TROUBLESHOOT
					   The object indicated has been flagged by the module which implements its class as one which must be named
					   to work properly.  Please provide the object with a name and try again.
					 */
				}
			}
		}
	} CATCH (char *msg) {
		output_error("init failure: %s", msg);
		/* TROUBLESHOOT
			The initialization procedure failed.  This is usually preceded 
			by a more detailed message that explains why it failed.  Follow
			the guidance for that message and try again.
		 */
		rv = FAILED;
	} ENDCATCH;
	return rv;
}

static STATUS commit_all(TIMESTAMP t0){
	OBJECT *obj;
	STATUS result = SUCCESS;
	TRY {
		for (obj=object_get_first(); obj!=NULL; obj=object_get_next(obj))
		{
			if(obj->in_svc <= t0 && obj->out_svc >= t0){
				if(object_commit(obj) == FAILED){
					THROW("commit_all(): object %s commit failed", object_name(obj));
					/* TROUBLESHOOT
						The commit function of the named object has failed.  Make sure that the object's
						requirements for commit'ing are satisfied and try again.  (likely internal state aberations)
					 */
				}
			}
		}
	} CATCH(char *msg){
		output_error("commit() failure: %s", msg);
		/* TROUBLESHOOT
			The commit'ing procedure failed.  This is usually preceded 
			by a more detailed message that explains why it failed.  Follow
			the guidance for that message and try again.
		 */
		result = FAILED;
	} ENDCATCH;
	return result;
}

STATUS exec_test(struct sync_data *data, int pass, OBJECT *obj);
 
STATUS t_setup_ranks(void){
	return setup_ranks();
}
STATUS t_sync_all(PASSCONFIG pass)
{
	struct sync_data sync = {TS_NEVER,0,SUCCESS};
	TIMESTAMP start_time = global_clock;
	int pass_index = ((int)(pass/2)); /* 1->0, 2->1, 4->2; NB: if a fourth pass is added this won't work right */

	/* scan the ranks of objects */
	if (ranks[pass_index] != NULL)
	{
		int i;

		/* process object in order of rank using index */
		for (i = PASSINIT(pass_index); PASSCMP(i, pass_index); i += PASSINC(pass_index))
		{
			LISTITEM *item;
			/* skip empty lists */
			if (ranks[pass_index]->ordinal[i] == NULL) 
				continue;
			
			
			for (item=ranks[pass_index]->ordinal[i]->first; item!=NULL; item=item->next)
			{
				OBJECT *obj = item->data;
				if (exec_test(&sync,pass,obj)==FAILED)
					return FAILED;
			}
		}
	}

	/* run all non-schedule transforms */
	{
		TIMESTAMP st = scheduletransform_syncall(global_clock,XS_DOUBLE|XS_COMPLEX|XS_ENDUSE);// if (abs(t)<t2) t2=t;
		if (st<sync.step_to)
			sync.step_to = st;
	}

	return SUCCESS;
}

/* this function synchronizes all internal behaviors */
TIMESTAMP syncall_internals(TIMESTAMP t1)
{
	TIMESTAMP sc, ls, st, eu, t2;
	sc = schedule_syncall(t1);
	//sc = schedule_syncall_ss(t1);
	ls = loadshape_syncall(t1);// if (abs(t)<t2) t2=t;
	st = scheduletransform_syncall(t1,XS_SCHEDULE|XS_LOADSHAPE);// if (abs(t)<t2) t2=t;

	eu = enduse_syncall(t1);// if (abs(t)<t2) t2=t;
	/* @todo add other internal syncs here */
	t2 = TS_NEVER;
	if(sc < t2) t2 = sc;
	if(ls < t2) t2 = ls;
	if(st < t2) t2 = st;
	if(eu < t2) t2 = eu;
	return t2;
}

/** This is the main simulation loop
	@return STATUS is SUCCESS if the simulation reached equilibrium, 
	and FAILED if a problem was encountered.
 **/
STATUS exec_start(void)
{
	//sjin: remove threadpool
	//threadpool_t threadpool = INVALID_THREADPOOL;
	struct sync_data sync = {TS_NEVER,0,SUCCESS};
	TIMESTAMP start_time = global_clock;
	int64 passes = 0, tsteps = 0;
	time_t started_at = realtime_now();
	int j, k;

	//sjin: implement pthreads
	pthread_t *thread_id;
	//sjin: add some variables for pthread implementation
	LISTITEM *ptr;
	int iPtr, incr;
	struct arg_data *arg_data_array;

	OBJECT *obj;

	/* check for a model */
	if (object_get_count()==0)

		/* no object -> nothing to do */
		return SUCCESS;

	/* perform object initialization */
	if (init_all() == FAILED)
	{
		output_error("model initialization failed");
		/* TROUBLESHOOT
			The initialization procedure failed.  This is usually preceded 
			by a more detailed message that explains why it failed.  Follow
			the guidance for that message and try again.
		 */
		return FAILED;
	}

	/* establish rank index if necessary */
	if (ranks == NULL && setup_ranks() == FAILED)
	{
		output_error("ranks setup failed");
		/* TROUBLESHOOT
			The rank setup procedure failed.  This is usually preceded 
			by a more detailed message that explains why it failed.  Follow
			the guidance for that message and try again.
		 */
		return FAILED;
	}

	//sjin: print out obj information
	//for (obj=object_get_first(); obj!=NULL; obj=object_get_next(obj))
		//printf("obj id: %d, name = %s, rank = %d\n", obj->id, obj->name, obj->rank);

	/* run checks */
	if (global_runchecks)
		return module_checkall();

	/* compile only check */
	if (global_compileonly)
		return SUCCESS;

	/* enable non-determinism check, if any */
	if (global_randomseed!=0 && global_threadcount>1)
		global_nondeterminism_warning = 1;

	if (!global_debug_mode)
	{
		/* schedule progress report event */
		if(global_show_progress){
			realtime_schedule_event(realtime_now()+1,show_progress);
		}

		/* set thread count equal to processor count if not passed on command-line */
		if (global_threadcount == 0)
			global_threadcount = processor_count();
		output_verbose("detected %d processor(s)", processor_count());
		output_verbose("using %d helper thread(s)", global_threadcount);

		//sjin: allocate arg_data_array to store pthreads creation argument
		arg_data_array = (struct arg_data *) malloc(sizeof(struct arg_data) 
						 * global_threadcount);

		/* allocate thread synchronization data */
		thread_data = (struct thread_data *) malloc(sizeof(struct thread_data) +
					  sizeof(struct sync_data) * global_threadcount);
		if (!thread_data) {
			output_error("thread memory allocation failed");
			/* TROUBLESHOOT
				A thread memory allocation failed.  
				Follow the standard process for freeing up memory ang try again.
			 */
			return FAILED;
		}
		thread_data->count = global_threadcount;
		thread_data->data = (struct sync_data *) (thread_data + 1);
		for (j = 0; j < thread_data->count; j++) 
			thread_data->data[j].status = SUCCESS;
	}
	else
	{
		output_debug("debug mode running single threaded");
		output_message("GridLAB-D entering debug mode");
	}

	/* realtime startup */
	if (global_run_realtime>0)
	{
		char buffer[64];
		time(&global_clock);
		output_verbose("realtime mode requires using now (%s) as starttime", convert_from_timestamp(global_clock,buffer,sizeof(buffer))>0?buffer:"invalid time");
		if (global_stoptime<global_clock)
			global_stoptime=TS_NEVER;
	}

	iteration_counter = global_iteration_limit;
	sync.step_to = global_clock;
	sync.hard_event = 1;

	/* signal handler */
	signal(SIGABRT, exec_sighandler);
	signal(SIGINT, exec_sighandler);
	signal(SIGTERM, exec_sighandler);

	//sjin: GetMachineCycleCount
	//mc_start_time = GetMachineCycleCount();
	start = clock();
	thread_id = (pthread_t *) malloc(global_threadcount * sizeof(pthread_t));
	/* main loop exception handler */
	TRY {

		/* main loop runs for iteration limit, or when nothing futher occurs (ignoring soft events) */
		int running; /* split into two tests to make it easier to tell what's going on */
		while ( running = (sync.step_to <= global_stoptime && sync.step_to < TS_NEVER && sync.hard_event>0),
			iteration_counter>0 && ( running || global_run_realtime>0) && !stop_now ) 
		{
			//printf("Iteration increased!\n\n");
			/* set time context */
			output_set_time_context(sync.step_to);

			sync.hard_event = (global_stoptime == TS_NEVER ? 0 : 1);

			/* realtime support */
			if (global_run_realtime>0)
			{
#ifdef WIN32
				struct timeb tv;
				ftime(&tv);
				output_verbose("waiting %d msec", 1000-tv.millitm);
				Sleep(1000-tv.millitm );
				if ( global_run_realtime==1 )
					global_clock = tv.time + global_run_realtime;
				else
					global_clock += global_run_realtime;
#else
				struct timeval tv;
				gettimeofday(&tv);
				output_verbose("waiting %d usec", 1000000-tv.tv_usec);
				usleep(1000000-tv.tv_usec);
				if ( global_run_realtime==1 )
					global_clock = tv.tv_sec+global_run_realtime;
				else
					global_clock += global_run_realtime;
#endif
				output_verbose("realtime clock advancing to %d", (int)global_clock);
			}
			else
				global_clock = sync.step_to;

			/* synchronize all internal schedules */
			sync.step_to = syncall_internals(global_clock);
			if(sync.step_to!=TS_NEVER && sync.step_to <= global_clock){
				THROW("internal property sync failure");
				/* TROUBLESHOOT
					An internal property such as schedule, enduse or loadshape has failed to synchronize and the simulation aborted.
					This message should be preceded by a more informative message that explains which element failed and why.
					Follow the troubleshooting recommendations for that message and try again.
				 */
			}

			if (!global_debug_mode)
			{
				for (j = 0; j < thread_data->count; j++) {
					thread_data->data[j].hard_event = 0;
					thread_data->data[j].step_to = TS_NEVER;
				}
			}
			/* scan the ranks of objects */
			for (pass = 0; ranks[pass] != NULL; pass++)
			{
				int i;

				/* process object in order of rank using index */
				for (i = PASSINIT(pass); PASSCMP(i, pass); i += PASSINC(pass))
				{
					/* skip empty lists */
					if (ranks[pass]->ordinal[i] == NULL) 
						continue;
					
					if (global_debug_mode)
					{
						LISTITEM *item;
						for (item=ranks[pass]->ordinal[i]->first; item!=NULL; item=item->next)
						{
							OBJECT *obj = item->data;
							if (exec_debug(&sync,pass,i,obj)==FAILED)
								THROW("debugger quit");
						}
					}
					else
					{
						//sjin: remove threadpool
						//tp_exec(threadpool, ranks[pass]->ordinal[i]);

						//sjin: if global_threadcount == 1, no pthread multhreading
						if (global_threadcount == 1) {
							for (ptr = ranks[pass]->ordinal[i]->first; ptr != NULL; ptr=ptr->next) {
								OBJECT *obj = ptr->data;
								ss_do_object_sync(0, ptr->data);					
								//printf("%d %s %d\n", obj->id, obj->name, obj->rank);
							}
							//printf("\n");
						} else { //sjin: implement pthreads
							incr = (int)ceil((float) ranks[pass]->ordinal[i]->size / global_threadcount);
							//printf("pass %d, incr = %d, size = %d\n", pass, incr, ranks[pass]->ordinal[i]->size);
							//thread_id = (pthread_t *) malloc(global_threadcount * sizeof(pthread_t));
							// copying this further up
							/* if the number of objects is less than or equal to the number of threads, each thread process one object */
							if (incr <= 1) {
								//printf("incr = %d\n", incr);
								iPtr = -1; 
								for (ptr = ranks[pass]->ordinal[i]->first; ptr != NULL; ptr = ptr->next) {
									iPtr ++;
									//printf("<= %d: ranks[%d]->ordinal[%d]->size = %d\n", iPtr, pass, i, ranks[pass]->ordinal[i]->size);
									arg_data_array[iPtr].thread = iPtr;
									arg_data_array[iPtr].item = ptr;
									arg_data_array[iPtr].incr = 1;
									//printf("<=1: size = %d, iPtr = %d, incr = %d, thread id = %d\n", ranks[pass]->ordinal[i]->size, iPtr, incr, &thread_id[iPtr]);
									pthread_create(&thread_id[iPtr], NULL, ss_do_object_sync_list, (void *) &arg_data_array[iPtr]);
									//pthread_join(thread_id[iPtr], NULL);
									//pthread_cancel(thread_id[iPtr]);
									//printf("<=1:\tthread_id[%d].p = %p\t.x = %lu\n", iPtr, (void *) thread_id[iPtr].p, (unsigned long) thread_id[iPtr].x);
								}
								for (j = 0; j < iPtr+1; j++) 
									pthread_join(thread_id[j], NULL);
							/* if the number of objects is greater than the number of threads, each thread process the same number of 
							   objects (incr), except that the last thread may process less objects */
							} else {
								//printf("incr = %d\n", incr);
								iPtr = -1;
								j = 0;
								for (ptr = ranks[pass]->ordinal[i]->first; ptr != NULL; ptr = ptr->next) {
									iPtr ++;
									if (iPtr % incr == 0) {
										//printf("> thread %d, start from object %d: ranks[%d]->ordinal[%d]->size = %d\n", j, iPtr, pass, i, ranks[pass]->ordinal[i]->size);
										arg_data_array[j].thread = j;
										arg_data_array[j].item = ptr;
										arg_data_array[j].incr = incr;
										//printf(">1: size = %d, iPtr = %d, incr = %d, thread id = %d\n", ranks[pass]->ordinal[i]->size, iPtr, incr, thread_id[j]);
										pthread_create(&thread_id[j], NULL, ss_do_object_sync_list, (void *) &arg_data_array[j]);
										//pthread_join(thread_id[j], NULL);
										//pthread_cancel(thread_id[j]);
										//printf("> 1:\tthread_id[%d].p = %p\t.x = %lu\n", j, (void *) thread_id[j].p, (unsigned long) thread_id[j].x);
										j++;
									}
								}
								for (k = 0; k < j; k++) {
									//printf("k=%d, j=%d\n",k,j);
									pthread_join(thread_id[k], NULL);
									//printf("done!\n");
								}
							}

							//if (iPtr+1 < global_threadcount) 
							/*if (incr <= 1) 
								for (j = 0; j < iPtr+1; j++) 
									pthread_join(thread_id[j], NULL);
							else
								for (j = 0; j < global_threadcount; j++) 
									pthread_join(thread_id[j], NULL);*/
						} //sjin: end of implementing pthreads

						for (j = 0; j < thread_data->count; j++) {
							if (thread_data->data[j].status == FAILED) {
								sync.status = FAILED;
								THROW("synchonization failed");
							}
						}
					}
				}

				/* run all non-schedule transforms */
				{
					TIMESTAMP st = scheduletransform_syncall(global_clock,XS_DOUBLE|XS_COMPLEX|XS_ENDUSE);// if (abs(t)<t2) t2=t;
					if (st<sync.step_to)
						sync.step_to = st;
				}
			}

			if (!global_debug_mode)
			{
				for (j = 0; j < thread_data->count; j++) {
					sync.hard_event += thread_data->data[j].hard_event;
					if (thread_data->data[j].step_to < sync.step_to)
						sync.step_to = thread_data->data[j].step_to;
				}

				/* report progress */
				realtime_run_schedule();
			}

			/* count number of passes */
			passes++;

			/* check for clock advance */
			if (sync.step_to != global_clock)
			{
				if (commit_all(global_clock) == FAILED)
				{
					output_error("model commit failed");
					/* TROUBLESHOOT
						The commit procedure failed.  This is usually preceded 
						by a more detailed message that explains why it failed.  Follow
						the guidance for that message and try again.
					 */
					return FAILED;
				}

				/* reset iteration count */
				iteration_counter = global_iteration_limit;

				/* count number of timesteps */
				tsteps++;
			}
			/* check iteration limit */
			else if (--iteration_counter == 0)
			{
				output_error("convergence iteration limit reached at %s (exec)", simtime());
				/* TROUBLESHOOT
					This indicates that the core's solver was unable to determine
					a steady state for all objects for any time horizon.  Identify
					the object that is causing the convergence problem and contact
					the developer of the module that implements that object's class.
				 */
				sync.status = FAILED;
				THROW("convergence failure");
			}
		}

		/* disable signal handler */
		signal(SIGINT,NULL);

		/* check end state */
		if (sync.step_to==TS_NEVER)
		{
			char buffer[64];
			output_verbose("simulation at steady state at %s", convert_from_timestamp(global_clock,buffer,sizeof(buffer))?buffer:"invalid time");
		}
	}
	CATCH(char *msg)
	{
		output_error("exec halted: %s", msg);
		sync.status = FAILED;
		/* TROUBLESHOOT
			This indicates that the core's solver shut down.  This message
			is usually preceded by more detailed messages.  Follow the guidance
			for those messages and try again.
		 */
	}
	ENDCATCH

	//sjin: GetMachineCycleCount
	//mc_end_time = GetMachineCycleCount();
	//printf("%ld\n",(mc_end_time-mc_start_time));
	end = clock();
	//printf("%f\n", (double)(end - start) / (double)CLOCKS_PER_SEC);

	/* deallocate threadpool */
	if (!global_debug_mode)
	{
		//sjin: remove threadpool
		//tp_release(threadpool);
		free(thread_data);

#ifdef NEVER
		/* wipe out progress report */
		if (!global_keep_progress)
			output_raw("                                                           \r"); 
#endif
	}

	/* report performance */
	if (global_profiler && sync.status==SUCCESS)
	{
		double elapsed_sim = (timestamp_to_hours(global_clock<start_time?start_time:global_clock)-timestamp_to_hours(start_time));
		double elapsed_wall = (double)(realtime_now()-started_at+1);
		double sync_time = 0;
		double sim_speed = object_get_count()/1000.0*elapsed_sim/elapsed_wall;
		CLASS *cl;
		if (global_threadcount==0) global_threadcount=1;
		for (cl=class_get_first_class(); cl!=NULL; cl=cl->next)
			sync_time += ((double)cl->profiler.clocks)/CLOCKS_PER_SEC;
		sync_time /= global_threadcount;

		output_profile("\nCore profiler results");
		output_profile("======================\n");
		output_profile("Total objects           %8d objects", object_get_count());
		output_profile("Parallelism             %8d thread%s", global_threadcount,global_threadcount>1?"s":"");
		output_profile("Total time              %8.1f seconds", elapsed_wall);
		output_profile("  Core time             %8.1f seconds (%.1f%%)", (elapsed_wall-sync_time),(elapsed_wall-sync_time)/elapsed_wall*100);
		output_profile("  Model time            %8.1f seconds/thread (%.1f%%)", sync_time,sync_time/elapsed_wall*100);
		output_profile("Simulation time         %8.0f days", elapsed_sim/24);
		if (sim_speed>10.0)
			output_profile("Simulation speed        %7.0lfk object.hours/second", sim_speed);
		else if (sim_speed>1.0)
			output_profile("Simulation speed        %7.1lfk object.hours/second", sim_speed);
		else
			output_profile("Simulation speed        %7.0lf object.hours/second", sim_speed*1000);
		output_profile("Syncs completed         %8d passes", passes);
		output_profile("Time steps completed    %8d timesteps", tsteps);
		output_profile("Convergence efficiency  %8.02lf passes/timestep", (double)passes/tsteps);
#ifndef NOLOCKS
		output_profile("Memory lock contention  %7.01lf%%", (lock_spin>0 ? (1-(double)lock_count/(double)lock_spin)*100 : 0));
#endif
		output_profile("Average timestep        %7.0lf seconds/timestep", (double)(global_clock<start_time?0:global_clock-start_time)/tsteps);
		output_profile("Simulation rate         %7.0lf x realtime", (double)(global_clock<start_time?0:global_clock-start_time)/elapsed_wall);
		output_profile("\n");
	}

	return sync.status;
}

/** Starts the executive test loop 
	@return STATUS is SUCCESS if all test passed, FAILED is any test failed.
 **/
STATUS exec_test(struct sync_data *data, /**< the synchronization state data */
				 int pass, /**< the pass number */
				 OBJECT *obj){ /**< the current object */
	TIMESTAMP this_t;
	/* check in and out-of-service dates */
	if (global_clock<obj->in_svc)
		this_t = obj->in_svc; /* yet to go in service */
	else if (global_clock<=obj->out_svc)
		this_t = object_sync(obj, global_clock, pass);
	else 
		this_t = TS_NEVER; /* already out of service */

	/* check for "soft" event (events that are ignored when stopping) */
	if (this_t < -1)
		this_t = -this_t;
	else if (this_t != TS_NEVER)
		data->hard_event++;  /* this counts the number of hard events */

	/* check for stopped clock */
	if (this_t < global_clock) {
		output_error("%s: object %s stopped its clock! (test)", simtime(), object_name(obj));
		/* TROUBLESHOOT
			This indicates that one of the objects in the simulator has encountered a
			state where it cannot calculate the time to the next state.  This usually
			is caused by a bug in the module that implements that object's class.
		 */
		data->status = FAILED;
	} else {
		/* check for iteration limit approach */
		if (iteration_counter == 2 && this_t == global_clock) {
			output_verbose("%s: object %s iteration limit imminent",
								simtime(), object_name(obj));
		}
		else if (iteration_counter == 1 && this_t == global_clock) {
			output_error("convergence iteration limit reached for object %s:%d (test)", obj->oclass->name, obj->id);
			/* TROUBLESHOOT
				This indicates that one of the objects in the simulator has encountered a
				state where it cannot calculate the time to the next state.  This usually
				is caused by a bug in the module that implements that object's class.
			 */
		}

		/* manage minimum timestep */
		if (global_minimum_timestep>1 && this_t>global_clock && this_t<TS_NEVER)
			this_t = ((this_t/global_minimum_timestep)+1)*global_minimum_timestep;

		/* if this event precedes next step, next step is now this event */
		if (data->step_to > this_t)
			data->step_to = this_t;
		data->status = SUCCESS;
	}
	return data->status;
}

/**@}*/
