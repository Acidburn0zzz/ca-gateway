// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$
// Revision 1.4  1996/09/10 15:04:14  jbk
// many fixes.  added instructions to usage. fixed exist test problems.
//
// Revision 1.3  1996/07/26 02:34:47  jbk
// Interum step.
//
// Revision 1.2  1996/07/23 16:32:44  jbk
// new gateway that actually runs
//

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "gateResources.h"

void gatewayServer(void);
void print_instructions(void);

// still need to add client and server IP addr info using 
// the CA environment variables.

// still need to add ability to load and run user servers dynamically

#if 0
void* operator new(size_t x)
{
	void* y = (void*)malloc(x);
	fprintf(stderr,"in op new for %d %8.8x\n",(int)x,y);
	return y;
}
void operator delete(void* x)
{
	fprintf(stderr,"in op del for %8.8x\n",x);
	free((char*)x);
}
#endif

// The parameters past in from the user are:
//	-debug ? = set debug level, ? is the integer level to set
//	-pv file_name = process variable list file
//	-log file_name = log file name
//	-access file_name = access security file
//	-home directory = the program's home directory
//	-connect_timeout number = clear PV connect requests every number seconds
//	-inactive_timeout number = Hold inactive PV connections for number seconds
//	-dead_timeout number = Hold PV connections with no user for number seconds
//	-cip ip_addr = CA client library IP address list (exclusive)
//	-sip ip_addr = IP address where CAS listens for requests
//	-cport port_number = CA client library port
//	-sport port_number = CAS port number
//	-? = display usage
//
//	GATEWAY_HOME = environement variable pointing to the home of the gateway
//
// Defaults:
//	Home directory = .
//	Access security file = GATEWAY_PV_ACCESS_FILE
//	process variable list file = GATEWAY_PV_LIST_FILE
//	log file = GATEWAY_LOG.xx (xx=[client,server])
//	debug level = 0 (none)
//  connect_timeout = 1 second
//  inactive_timeout = 60*60*2 seconds (2 hours)
//  dead_timeout = 60*2 seconds (2 minutes)
//	cport/sport = CA default port number
//	cip = nothing, the normal interface
//	sip = nothing, the normal interface
//
// Precidence:
//	(1) Command line parameter 
//	(2) environment variables
//	(3) defaults

#define PARM_DEBUG			0
#define PARM_PV				1
#define PARM_LOG			2
#define PARM_ACCESS			3
#define PARM_HOME			4
#define PARM_ALIAS			5
#define PARM_CONNECT		6
#define PARM_INACTIVE		7
#define PARM_DEAD			8
#define PARM_USAGE			9
#define PARM_SERVER_IP		10
#define PARM_CLIENT_IP		11
#define PARM_SERVER_PORT	12
#define PARM_CLIENT_PORT	13
#define PARM_HELP			14
#define PARM_NS				15

static char gate_ca_auto_list[] = "EPICS_CA_AUTO_ADDR_LIST=NO";
static char* server_ip_addr=NULL;
static char* client_ip_addr=NULL;
static int server_port=0;
static int client_port=0;
static int no_server=0;

struct parm_stuff
{
	char* parm;
	int len;
	int id;
	char* desc;
};
typedef struct parm_stuff PARM_STUFF;

static PARM_STUFF ptable[] = {
	{ "-debug",				6,	PARM_DEBUG,			"value" },
	{ "-pv",				3,	PARM_PV,			"file_name" },
	{ "-alias",				6,	PARM_ALIAS,			"file_name" },
	{ "-log",				4,	PARM_LOG,			"file_name" },
	{ "-access",			7,	PARM_ACCESS,		"file_name" },
	{ "-home",				5,	PARM_HOME,			"directory" },
	{ "-sip",				4,	PARM_SERVER_IP,		"IP_address" },
	{ "-cip",				4,	PARM_CLIENT_IP,		"IP_address_list" },
	{ "-sport",				6,	PARM_SERVER_PORT,	"CA_server_port" },
	{ "-cport",				6,	PARM_CLIENT_PORT,	"CA_client_port" },
	{ "-connect_timeout",	16,	PARM_CONNECT,		"seconds" },
	{ "-inactive_timeout",	17,	PARM_INACTIVE,		"seconds" },
	{ "-dead_timeout",		13,	PARM_DEAD,			"seconds" },
	{ "-noserver",			9,	PARM_NS,			"(start interactively)" },
	{ "-ns",				3,	PARM_NS,			NULL },
	{ "-help",				5,	PARM_HELP,			NULL },
	{ NULL,			-1,	-1 }
};

static int startEverything(void)
{
	char gate_cas_port[30];
	char gate_cas_addr[50];
	char gate_ca_list[100];
	char gate_ca_port[30];
	int sid;
	FILE* fd;

	if(client_ip_addr)
	{
		sprintf(gate_ca_list,"EPICS_CA_ADDR_LIST=%s",client_ip_addr);
		putenv(gate_ca_auto_list);
		putenv(gate_ca_list);
		gateDebug1(15,"gateway setting <%s>\n",gate_ca_list);
	}

	if(server_ip_addr)
	{
		sprintf(gate_cas_addr,"EPICS_CAS_ADDR=%s",server_ip_addr);
		putenv(gate_cas_addr);
		gateDebug1(15,"gateway setting <%s>\n",gate_cas_addr);
	}

	if(client_port)
	{
		sprintf(gate_ca_port,"EPICS_CA_SERVER_PORT=%d",client_port);
		putenv(gate_ca_port);
		gateDebug1(15,"gateway setting <%s>\n",gate_ca_port);
	}

	if(server_port)
	{
		sprintf(gate_cas_port,"EPICS_CAS_SERVER_PORT=%d",server_port);
		putenv(gate_cas_port);
		gateDebug1(15,"gateway setting <%s>\n",gate_cas_port);
	}

	if((fd=fopen(GATE_SCRIPT_FILE,"w"))==(FILE*)NULL)
	{
		fprintf(stderr,"open of script file %s failed\n",
			GATE_SCRIPT_FILE);
		fd=stderr;
	}

	sid=getpid();
	
	fprintf(fd,"\n");
	fprintf(fd,"# option:\n");
	fprintf(fd,"# home=<%s>\n",global_resources->homeDirectory());
	fprintf(fd,"# access file=<%s>\n",global_resources->accessFile());
	fprintf(fd,"# list file=<%s>\n",global_resources->listFile());
	fprintf(fd,"# alias file=<%s>\n",global_resources->aliasFile());
	fprintf(fd,"# log file=<%s>\n",global_resources->logFile());
	fprintf(fd,"# debug level=%d\n",global_resources->debugLevel());
	fprintf(fd,"# dead t-out=%d\n",global_resources->deadTimeout());
	fprintf(fd,"# connect t-out=%d\n",global_resources->connectTimeout());
	fprintf(fd,"# inactive t-out=%d\n",global_resources->inactiveTimeout());
	if(client_ip_addr)
	{
		fprintf(fd,"# %s\n",gate_ca_list);
		fprintf(fd,"# %s\n",gate_ca_auto_list);
	}
	if(server_ip_addr) fprintf(fd,"# %s\n",gate_cas_addr);
	if(client_port) fprintf(fd,"# %s\n",gate_ca_port);
	if(server_port) fprintf(fd,"# %s\n",gate_cas_port);
	fprintf(fd,"\n");
	fprintf(fd,"kill %d\n",sid);
	fprintf(fd,"\n");
	fflush(fd);
	
	if(fd!=stderr) fclose(fd);
	chmod(GATE_SCRIPT_FILE,00755);
	
	gatewayServer();
	return 0;
}

int main(int argc, char** argv)
{
	int level,i,j,not_done,no_error,connect_tout,inactive_tout,dead_tout;
	char* home_dir;

	global_resources = new gateResources;
	gateResources* gr = global_resources;

	if(home_dir=getenv("GATEWAY_HOME"))
		gr->setHome(home_dir);

	not_done=1; no_error=1;
	for(i=1;i<argc && no_error;i++)
	{
		for(j=0;not_done && no_error && ptable[j].parm;j++)
		{
			if(strncmp(ptable[j].parm,argv[i],ptable[j].len)==0)
			{
				switch(ptable[j].id)
				{
				case PARM_DEBUG:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&level)<1)
								no_error=0;
							else
							{
								not_done=0;
								gr->setDebugLevel(level);
							}
						}
					}
					break;
				case PARM_HELP:
					print_instructions();
					return 0;
				case PARM_NS:
					no_server=1;
					not_done=0;
					break;
				case PARM_PV:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							gr->setListFile(argv[i]);
							not_done=0;
						}
					}
					break;
				case PARM_ALIAS:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							gr->setAliasFile(argv[i]);
							not_done=0;
						}
					}
					break;
				case PARM_LOG:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							gr->setLogFile(argv[i]);
							not_done=0;
						}
					}
					break;
				case PARM_ACCESS:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							gr->setAccessFile(argv[i]);
							not_done=0;
						}
					}
					break;
				case PARM_HOME:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							gr->setHome(argv[i]);
							not_done=0;
						}
					}
					break;
				case PARM_SERVER_IP:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							server_ip_addr=argv[i];
							not_done=0;
						}
					}
					break;
				case PARM_CLIENT_IP:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							client_ip_addr=argv[i];
							not_done=0;
						}
					}
					break;
				case PARM_CLIENT_PORT:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&client_port)<1)
								no_error=0;
							else
								not_done=0;
						}
					}
					break;
				case PARM_SERVER_PORT:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&server_port)<1)
								no_error=0;
							else
								not_done=0;
						}
					}
					break;
				case PARM_DEAD:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&dead_tout)<1)
								no_error=0;
							else
							{
								gr->setDeadTimeout(dead_tout);
								not_done=0;
							}
						}
					}
					break;
				case PARM_INACTIVE:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&inactive_tout)<1)
								no_error=0;
							else
							{
								not_done=0;
								gr->setInactiveTimeout(inactive_tout);
							}
						}
					}
					break;
				case PARM_CONNECT:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&connect_tout)<1)
								no_error=0;
							else
							{
								not_done=0;
								gr->setConnectTimeout(connect_tout);
							}
						}
					}
					break;
				default:
					no_error=0;
					break;
				}
			}
		}
		not_done=1;
		if(ptable[j].parm==NULL) no_error=0;
	}

	if(no_error==0)
	{
		int ii;
		fprintf(stderr,"usage: %s followed by the these options:\n",argv[0]);
		for(ii=0;ptable[ii].parm;ii++)
		{
			if(ptable[ii].desc)
				fprintf(stderr,"\t[%s %s ]\n",ptable[ii].parm,ptable[ii].desc);
			else
				fprintf(stderr,"\t[%s]\n",ptable[ii].parm);
		}
		fprintf(stderr,"\nDefaults are:\n");
		fprintf(stderr,"\tdebug=%d\n",gr->debugLevel());
		fprintf(stderr,"\thome=%s\n",gr->homeDirectory());
		fprintf(stderr,"\taccess=%s\n",gr->accessFile());
		fprintf(stderr,"\talias=%s\n",gr->aliasFile());
		fprintf(stderr,"\tpv=%s\n",gr->listFile());
		fprintf(stderr,"\tlog=%s\n",gr->logFile());
		fprintf(stderr,"\tdead=%d\n",gr->deadTimeout());
		fprintf(stderr,"\tconnect=%d\n",gr->connectTimeout());
		fprintf(stderr,"\tinactive=%d\n",gr->inactiveTimeout());
		return -1;
	}

	if(gr->debugLevel()>10)
	{
		fprintf(stderr,"\noption dump:\n");
		fprintf(stderr," home=<%s>\n",gr->homeDirectory());
		fprintf(stderr," access file=<%s>\n",gr->accessFile());
		fprintf(stderr," list file=<%s>\n",gr->listFile());
		fprintf(stderr," log file=<%s>\n",gr->logFile());
		fprintf(stderr," alias file=<%s>\n",gr->aliasFile());
		fprintf(stderr," debug level=%d\n",gr->debugLevel());
		fprintf(stderr," connect timeout =%d\n",gr->connectTimeout());
		fprintf(stderr," inactive timeout =%d\n",gr->inactiveTimeout());
		fprintf(stderr," dead timeout =%d\n",gr->deadTimeout());
		fflush(stderr);
	}

#ifdef DEBUG_MODE
	startEverything();
#else
	if(no_server)
		startEverything();
	else
	{
		gr->setUpLogging();

		// disassociate from parent
		switch(fork())
		{
		case -1: // error
			perror("Cannot create gateway processes");
			return -1;
		case 0: // child
#if defined linux || defined SOLARIS
			setpgrp();
#else
			setpgrp(0,0);
#endif
			setsid();
			startEverything();
			break;
		default: // parent
			break;
		}
	}
#endif
	delete global_resources;
	return 0;
}

#define pr fprintf

void print_instructions(void)
{
  pr(stderr,"-debug value: Enter value between 0-100.  50 gives lots of\n");
  pr(stderr," info, 1 gives small amount.\n\n");
  pr(stderr,"-pv file_name: File with list of valid PV names in it.  The\n");
  pr(stderr," list can contain wild cards.  Here is an example:\n");
  pr(stderr,"\tmotor_pv\n\tthing\n\tS1:*\n\tS2:*:motor\n");
  pr(stderr," A file with this info in it will allow clients to attach to\n");
  pr(stderr," PVs motor_pv, thing, anything starting with \"S1:\", and\n");
  pr(stderr," PVs starting with \"S2:\" and ending with \"motor\".\n\n");
  pr(stderr,"-alias file_name: File containing PV/alias pairs.  A list of\n");
  pr(stderr," fake PV names that the client can access and the real names\n");
  pr(stderr," that they get translated to.  Example:\n");
  pr(stderr,"\tgap_size S1:C6:F3:gap_size_A\n");
  pr(stderr,"\tcurrent S9:C3:F7:current_value\n");
  pr(stderr," Clients can now attach to PV S1:C6:F3:gap_size_A using alias\n");
  pr(stderr," PV gap_size and S9:C3:F7:current_value using PV current\n\n");
  pr(stderr,"-log file_name: Name of file where all messages from the\n");
  pr(stderr," gateway go, including stderr and stdout.\n\n");
  pr(stderr,"-access file_name: EPICS access security, not implemented.\n\n");
  pr(stderr,"-home directory: Home directory where all your gateway\n");
  pr(stderr," configuration files are kept and where the log file goes.\n\n");
  pr(stderr,"-sip IP_address: IP address that gateway's CA server listens\n");
  pr(stderr," for PV requests.  Sets environment variable EPICS_CAS_ADDR.\n\n");
  pr(stderr,"-cip IP_address_list: IP address list that the gateway's CA\n");
  pr(stderr," client uses to find the real PVs.  See CA reference manual.\n");
  pr(stderr," This sets environment variables EPICS_CA_AUTO_LIST=NO and\n");
  pr(stderr," EPICS_CA_ADDR_LIST.\n\n");
  pr(stderr,"-sport CA_server_port: The port which the gateway's CA server\n");
  pr(stderr," uses to listen for PV requests.  Sets environment variable\n");
  pr(stderr," EPICS_CAS_SERVER_PORT.\n\n");
  pr(stderr,"-cport CA_client_port:  The port thich the gateway's CA client\n");
  pr(stderr," uses to find the real PVs.  Sets environment variable\n");
  pr(stderr," EPICS_CA_SERVER_PORT.\n\n");
  pr(stderr,"-connect_timeout seconds: The amount of time that the\n");
  pr(stderr," gateway will allow a PV search to continue before marking the\n");
  pr(stderr," PV as being not found.\n\n");
  pr(stderr,"-inactive_timeout seconds: The amount of time that the gateway\n");
  pr(stderr," will hold the real connection to an unused PV.  If no gateway\n");
  pr(stderr," clients are using the PV, the real connection will still be\n");
  pr(stderr," held for this long.\n\n");
  pr(stderr,"-dead_timeout seconds:  The amount of time that the gateway\n");
  pr(stderr," will hold requests for PVs that are not found on the real\n");
  pr(stderr," network that the gateway is using.  Even if a client's\n");
  pr(stderr," requested PV is not found on the real network, the gateway\n");
  pr(stderr," marks the PV dead and holds the request and continues trying\n");
  pr(stderr," to connect for this long.\n\n");
  pr(stderr,"-noserver: Start the server interactively at the terminal.  Do\n");
  pr(stderr," not put the process in the background and do not detach it\n");
  pr(stderr," from the terminal\n\n");
  pr(stderr,"-ns: Same as -noserver.\n");
}

