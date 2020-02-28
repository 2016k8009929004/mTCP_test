#include "common.h"

//#define __REAL_TIME_STATS__

//#define __EVAL_HANDLE__

#define __EVAL_CYCLE__

#define BUF_SIZE 4096

#define MAX_FLOW_NUM  (10000)

#define MAX_EVENTS (MAX_FLOW_NUM * 3)

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#define HT_SUPPORT FALSE

#ifndef MAX_CPUS
#define MAX_CPUS		16
#endif

#ifndef MIN
#define MIN(v1, v2)	((v1) < (v2) ? (v1) : (v2))
#endif

struct thread_context {
	mctx_t mctx;
	int ep;
	struct server_vars *svars;
};

struct server_vars {
	int recv_len;
	int request_len;
	long int total_read, total_sent;
	uint8_t done;
	uint8_t rspheader_sent;
	uint8_t keep_alive;

	struct timeval start;
	int start_flag;

	int request_cnt;
	int byte_sent;

	int total_time;

	int cycle_cnt;
	int cycle_time;
};

static int num_cores;
static int core_limit;
static pthread_t app_thread[MAX_CPUS];
static int done[MAX_CPUS];
static char *conf_file = NULL;
static int backlog = -1;

void * server_thread(void * arg);