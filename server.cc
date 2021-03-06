#include "server.h"

struct timeval end_all;

#ifdef __EVAL_FRAM__
int trans_start_flag = 0;

int accept_time = 0;
int accept_cnt = 0;

int read_time = 0;
int read_cnt = 0;
#endif

int ZeroCopyProcess(struct thread_context *ctx, int thread_id, int sockid, struct server_vars *sv){
	int recv_len;
	char * recv_buff;

	struct mtcp_var * mvar = (struct mtcp_var *)GetRecvBuffer(ctx->mctx, sockid, &recv_len, &recv_buff);

	//printf(" >> recv len: %d\n", recv_len);

	if(recv_len == 0){
		return recv_len;
	}

	int res, to_send;

	to_send = 0;

	if(recv_len == KV_ITEM_SIZE){
		struct kv_trans_item * request = (struct kv_trans_item *)recv_buff;
	    res = hi->insert(thread_id, (uint8_t *)request->key, (uint8_t *)request->value);
    	//printf("[SERVER] put key: %.*s\nput value: %.*s\n", KEY_SIZE, request->key, VALUE_SIZE, request->value);
    
		to_send = REPLY_SIZE;
		char * send_buff = GetSendBuffer(mvar, to_send);
		if(!send_buff){
			perror("Get send buffer failed\n");
			return -1;
		}
		
		if (res == true){
    	    char message[] = "put success";
        	//memcpy(reply, message, strlen(message));
			//sent = mtcp_write(ctx->mctx, sockid, reply, REPLY_SIZE);
			memcpy(send_buff, message, strlen(message));
			//WriteProcess(mvar, strlen(message));
			//to_send += REPLY_SIZE;
	    }else{
    	    char message[] = "put failed";
        	//memcpy(reply, message, strlen(message));
			//sent = mtcp_write(ctx->mctx, sockid, reply, REPLY_SIZE);
			memcpy(send_buff, message, strlen(message));
			//WriteProcess(mvar, strlen(message));
			//to_send += REPLY_SIZE;
		}
	#ifdef __EVAL_KV__
        pthread_mutex_lock(&record_lock);
    	put_cnt++;
    	pthread_mutex_unlock(&record_lock);
	#endif
	}else if(recv_len == NUM_BATCH * KEY_SIZE){
	#ifdef __EVAL_KV__
        pthread_mutex_lock(&put_end_lock);
        if(!put_end_flag){
            gettimeofday(&put_end, NULL);
            put_end_flag = 1;
        }
        pthread_mutex_unlock(&put_end_lock);
    #endif
		int key_num = recv_len / KEY_SIZE;
		to_send = NUM_BATCH * VALUE_SIZE;
		char * send_buff = GetSendBuffer(mvar, to_send);

	    int i;
		for(i = 0;i < key_num;i++){
			//printf(" >> GET key: %.*s\n", KEY_SIZE, recv_buff + i * KEY_SIZE);
			res = hi->search(thread_id, (uint8_t *)(recv_buff + i * KEY_SIZE), (uint8_t *)(send_buff + i * VALUE_LENGTH));
			if(res == true){
	            //printf(" >> GET success! value: %.*s\n", VALUE_LENGTH, send_buff);
				//WriteProcess(mvar, VALUE_SIZE);
				//to_send += VALUE_SIZE;
        	}else{
            	//printf(" >> GET failed\n");
	    	    char message[VALUE_SIZE] = "get failed";
    	        memcpy(send_buff + i * VALUE_LENGTH, message, strlen(message));
				//WriteProcess(mvar, VALUE_SIZE);
				//to_send += VALUE_SIZE;
			}
		}
	#ifdef __EVAL_KV__
        pthread_mutex_lock(&record_lock);
    	get_cnt += key_num;
    	pthread_mutex_unlock(&record_lock);
    #endif
	}else if(recv_len == 2 * KEY_SIZE){
    #ifdef __EVAL_KV__
        pthread_mutex_lock(&get_end_lock);
        if(!get_end_flag){
            gettimeofday(&get_end, NULL);
            get_end_flag = 1;
        }
        pthread_mutex_unlock(&get_end_lock);
    #endif
        //printf(" >> SCAN key: %.*s, %.*s\n", KEY_SIZE, recv_item, KEY_SIZE, recv_item + KEY_SIZE);
        
        //char * scan_buff = (char *)malloc(scan_range * VALUE_LENGTH);
        char * scan_buff = (char *)malloc(sizeof(unsigned long *) * (sv->scan_range));

		to_send = (sv->scan_range - 1) * VALUE_LENGTH;
		char * send_buff = GetSendBuffer(mvar, to_send);

        int total_scan_count;
        if (memcmp(recv_buff, recv_buff + KEY_SIZE, KEY_SIZE) > 0){
            //key1 > key2
            total_scan_count = hi->range_scan((uint8_t *)(recv_buff + KEY_SIZE), (uint8_t *)recv_buff, scan_buff);
        }else{
            //key1 < key2
            total_scan_count = hi->range_scan((uint8_t *)recv_buff, (uint8_t *)(recv_buff + KEY_SIZE), scan_buff);
        }
        //printf(" >> SCAN total count: %d\n", total_scan_count);
        
        if(total_scan_count >= sv->scan_range){
            goto done;
        }

        int i;
        for(i = 0;i < total_scan_count;i++){
            unsigned long * ptr = (unsigned long *)scan_buff;
            struct kv_item * item = (struct kv_item *)ptr[i];
            memcpy(send_buff + i * VALUE_LENGTH, item->value, VALUE_LENGTH);
			//WriteProcess(mvar, VALUE_LENGTH);
            //printf(" >> SCAN value: %.*s\n", VALUE_LENGTH, value + i * VALUE_LENGTH);
        }
        
done:
        free(scan_buff);
    #ifdef __EVAL_KV__
        pthread_mutex_lock(&record_lock);
        scan_cnt += 1;
        pthread_mutex_unlock(&record_lock);
    #endif
	}
    
	int send_len = SendProcess(mvar, recv_len, to_send);

}

void CleanServerVariable(struct server_vars *sv){
	sv->recv_len = 0;
	sv->request_len = 0;
	sv->total_read = 0;
	sv->total_sent = 0;
	sv->done = 0;
	sv->rspheader_sent = 0;
	sv->keep_alive = 0;
	sv->total_time = 0;
//    sv->recv_buf = (struct ring_buf *)malloc(RING_BUF_SIZE);
//    init_ring_buff(sv->recv_buf);
	sv->scan_range = 4;
}

void CloseConnection(struct thread_context *ctx, int sockid, struct server_vars *sv){
#ifdef __REAL_TIME__
	pthread_mutex_lock(&end_lock);
    gettimeofday(&g_end, NULL);
    pthread_mutex_unlock(&end_lock);
#endif

#ifdef __EVAL_KV__
        pthread_mutex_lock(&end_lock);
        gettimeofday(&g_end, NULL);
        pthread_mutex_unlock(&end_lock);
#endif

#ifdef __EVAL_FRAM__
	trans_start_flag = 0;
#endif

	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);
	mtcp_close(ctx->mctx, sockid);
}

int HandleReadEvent(struct thread_context *ctx, int thread_id, int sockid, struct server_vars *sv){
#ifdef __TEST_FILE__
	int len, sent, recv_len;
	len = sent = 0;

    char * recv_item = (char *)malloc(BUF_SIZE);

#ifdef __EVAL_READ__
    struct timeval read_start;
    gettimeofday(&read_start, NULL);
#endif

	len = mtcp_recv(ctx->mctx, sockid, recv_item, BUF_SIZE, 0);

	if(len == 0){
		return len;
	}

#ifdef __EVAL_READ__
    struct timeval read_end;
    gettimeofday(&read_end, NULL);

    int start_time = read_start.tv_sec * 1000000 + read_start.tv_usec;
    int end_time = read_end.tv_sec * 1000000 + read_end.tv_usec;

    pthread_mutex_lock(&read_cb_lock);
    read_cnt++;
    read_time += (end_time - start_time);
    pthread_mutex_unlock(&read_cb_lock);
#endif

#ifdef __EVAL_READ__
    struct timeval write_start;
    gettimeofday(&write_start, NULL);
#endif

	mtcp_write(ctx->mctx, sockid, recv_item, len);

#ifdef __EVAL_READ__
    struct timeval write_end;
    gettimeofday(&write_end, NULL);

    start_time = write_start.tv_sec * 1000000 + write_start.tv_usec;
    end_time = write_end.tv_sec * 1000000 + write_end.tv_usec;

    pthread_mutex_lock(&read_cb_lock);
    write_cnt++;
    write_time += (end_time - start_time);
    pthread_mutex_unlock(&read_cb_lock);
#endif

#else
#ifdef __EVAL_FRAM__
    struct timeval start;
    gettimeofday(&start, NULL);
#endif

#ifdef __EVAL_CB__
    struct timeval start;
    gettimeofday(&start, NULL);
#endif

	int len, sent, recv_len;
	len = sent = 0;

    char * recv_item = (char *)malloc(BUF_SIZE);

#ifdef __EVAL_READ__
    struct timeval read_start;
    gettimeofday(&read_start, NULL);
#endif

	len = mtcp_recv(ctx->mctx, sockid, recv_item, BUF_SIZE, 0);

	if(len == 0){
		return len;
	}

#ifdef __EVAL_READ__
    struct timeval read_end;
    gettimeofday(&read_end, NULL);

    int start_time = read_start.tv_sec * 1000000 + read_start.tv_usec;
    int end_time = read_end.tv_sec * 1000000 + read_end.tv_usec;

    pthread_mutex_lock(&read_cb_lock);
    read_cnt++;
    read_time += (end_time - start_time);
    pthread_mutex_unlock(&read_cb_lock);
#endif

	int res;
    if(len == KV_ITEM_SIZE){
		struct kv_trans_item * request = (struct kv_trans_item *)recv_item;
        res = hi->insert(thread_id, (uint8_t *)request->key, (uint8_t *)request->value);
        //printf("[SERVER] put key: %.*s\nput value: %.*s\n", KEY_SIZE, recv_item->key, VALUE_SIZE, recv_item->value);
        char * reply = (char *)malloc(REPLY_SIZE);
        memset(reply, 0, REPLY_SIZE);
		if (res == true){
            char message[] = "put success";
            memcpy(reply, message, strlen(message));
			sent = mtcp_write(ctx->mctx, sockid, reply, REPLY_SIZE);
        }else{
            char message[] = "put failed";
            memcpy(reply, message, strlen(message));
			sent = mtcp_write(ctx->mctx, sockid, reply, REPLY_SIZE);
		}
		free(reply);

	#ifdef __EVAL_KV__
        pthread_mutex_lock(&record_lock);
    	put_cnt++;
    	pthread_mutex_unlock(&record_lock);
	#endif
    }else if(len == NUM_BATCH * KEY_SIZE){
    #ifdef __EVAL_KV__
        pthread_mutex_lock(&put_end_lock);
        if(!put_end_flag){
            gettimeofday(&put_end, NULL);
            put_end_flag = 1;
        }
        pthread_mutex_unlock(&put_end_lock);
    #endif
	
		int key_num = len / KEY_SIZE;
		char * value = (char *)malloc(key_num * VALUE_LENGTH);

        int i;
		for(i = 0;i < key_num;i++){
            //printf(" >> GET key: %.*s\n", KEY_SIZE, recv_item + i * KEY_SIZE);
			res = hi->search(thread_id, (uint8_t *)(recv_item + i * KEY_SIZE), (uint8_t *)(value + i * VALUE_LENGTH));
            if(res == true){
                //printf(" >> GET success! value: %.*s\n", VALUE_LENGTH, recv_item + i * KEY_SIZE);
            }else{
                //printf(" >> GET failed\n");
	            memset(value + i * VALUE_LENGTH, 0, VALUE_LENGTH);
    	        char message[] = "get failed";
        	    memcpy(value + i * VALUE_LENGTH, message, strlen(message));
			}
		}

		sent = mtcp_write(ctx->mctx, sockid, value, key_num * VALUE_LENGTH);

		free(value);

	#ifdef __EVAL_READ__
        struct timeval write_start;
        gettimeofday(&write_start, NULL);
    #endif

	#ifdef __EVAL_READ__
        struct timeval write_end;
        gettimeofday(&write_end, NULL);

        int start_time = write_start.tv_sec * 1000000 + write_start.tv_usec;
        int end_time = write_end.tv_sec * 1000000 + write_end.tv_usec;

        pthread_mutex_lock(&read_cb_lock);
        write_cnt++;
        write_time += (end_time - start_time);
        pthread_mutex_unlock(&read_cb_lock);
    #endif
	
	#ifdef __EVAL_KV__
        pthread_mutex_lock(&record_lock);
    	get_cnt += key_num;
    	pthread_mutex_unlock(&record_lock);
    #endif
    }else if(len == 2 * KEY_SIZE){
    #ifdef __EVAL_KV__
        pthread_mutex_lock(&get_end_lock);
        if(!get_end_flag){
            gettimeofday(&get_end, NULL);
            get_end_flag = 1;
        }
        pthread_mutex_unlock(&get_end_lock);
    #endif
        //printf(" >> SCAN key: %.*s, %.*s\n", KEY_SIZE, recv_item, KEY_SIZE, recv_item + KEY_SIZE);
        
        //char * scan_buff = (char *)malloc(scan_range * VALUE_LENGTH);
        char * scan_buff = (char *)malloc(sizeof(unsigned long *) * (sv->scan_range));

        int total_scan_count;
        if (memcmp(recv_item, recv_item + KEY_SIZE, KEY_SIZE) > 0){
            //key1 > key2
            total_scan_count = hi->range_scan((uint8_t *)(recv_item + KEY_SIZE), (uint8_t *)recv_item, scan_buff);
        }else{
            //key1 < key2
            total_scan_count = hi->range_scan((uint8_t *)recv_item, (uint8_t *)(recv_item + KEY_SIZE), scan_buff);
        }
        //printf(" >> SCAN total count: %d\n", total_scan_count);

        char * value = (char *)malloc((sv->scan_range - 1) * VALUE_LENGTH);
        memset(value, 0, (sv->scan_range - 1) * VALUE_LENGTH);
        
        if(total_scan_count >= sv->scan_range){
            goto done;
        }

        int i;
        for(i = 0;i < total_scan_count;i++){
            unsigned long * ptr = (unsigned long *)scan_buff;
            struct kv_item * item = (struct kv_item *)ptr[i];
            memcpy(value + i * VALUE_LENGTH, item->value, VALUE_LENGTH);
            //printf(" >> SCAN value: %.*s\n", VALUE_LENGTH, value + i * VALUE_LENGTH);
        }
        
done:
		sent = mtcp_write(ctx->mctx, sockid, value, (sv->scan_range - 1) * VALUE_LENGTH);
    
        free(scan_buff);
        free(value);
    #ifdef __EVAL_KV__
        pthread_mutex_lock(&record_lock);
        scan_cnt += 1;
        pthread_mutex_unlock(&record_lock);
    #endif
	}

    #ifdef __EVAL_CB__
        struct timeval end;
        gettimeofday(&end, NULL);
        double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
        double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

        pthread_mutex_lock(&read_lock);
        get_cnt++;
        get_time += (int)(end_time - start_time);
        pthread_mutex_unlock(&read_lock);
    #endif

	free(recv_item);

	//fclose(fp);

#ifdef __REAL_TIME__
    pthread_mutex_lock(&record_lock);
    request_cnt++;
    byte_sent += sent;
    pthread_mutex_unlock(&record_lock);
#endif

#ifdef __EVAL_FRAM__
    struct timeval end;
    gettimeofday(&end, NULL);

    double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
    double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

	read_cnt++;
	read_time += (int)(end_time - start_time);
#endif
#endif

    return len;
}

int AcceptConnection(struct thread_context *ctx, int listener){
	mctx_t mctx = ctx->mctx;
	struct server_vars *sv;
	struct mtcp_epoll_event ev;
	int c;

#ifdef __EVAL_FRAM__
	struct timeval start;
	gettimeofday(&start, NULL);
#endif

	c = mtcp_accept(mctx, listener, NULL, NULL);

	if (c >= 0) {
		if (c >= MAX_FLOW_NUM) {
			TRACE_ERROR("Invalid socket id %d.\n", c);
			return -1;
		}

		sv = &ctx->svars[c];
		CleanServerVariable(sv);
		TRACE_APP("New connection %d accepted.\n", c);
		ev.events = MTCP_EPOLLIN;
		ev.data.sockid = c;
		mtcp_setsock_nonblock(ctx->mctx, c);
		mtcp_epoll_ctl(mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, c, &ev);
		TRACE_APP("Socket %d registered.\n", c);

	} else {
		if (errno != EAGAIN) {
			TRACE_ERROR("mtcp_accept() error %s\n", 
					strerror(errno));
		}
	}

#ifdef __EVAL_FRAM__
	struct timeval end;
	gettimeofday(&end, NULL);

	double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
    double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

	accept_cnt++;
	accept_time += (int)(end_time - start_time);
#endif

	return c;
}

struct thread_context * InitializeServerThread(int core){
	struct thread_context *ctx;

	/* affinitize application thread to a CPU core */
#if HT_SUPPORT
	mtcp_core_affinitize(core + (num_cores / 2));
#else
	mtcp_core_affinitize(core);
#endif /* HT_SUPPORT */

	ctx = (struct thread_context *)calloc(1, sizeof(struct thread_context));
	if (!ctx) {
		TRACE_ERROR("Failed to create thread context!\n");
		return NULL;
	}

	/* create mtcp context: this will spawn an mtcp thread */
	ctx->mctx = mtcp_create_context(core);
	if (!ctx->mctx) {
		TRACE_ERROR("Failed to create mtcp context!\n");
		free(ctx);
		return NULL;
	}

	/* create epoll descriptor */
	ctx->ep = mtcp_epoll_create(ctx->mctx, MAX_EVENTS);
	if (ctx->ep < 0) {
		mtcp_destroy_context(ctx->mctx);
		free(ctx);
		TRACE_ERROR("Failed to create epoll descriptor!\n");
		return NULL;
	}

	/* allocate memory for server variables */
	ctx->svars = (struct server_vars *)
			calloc(MAX_FLOW_NUM, sizeof(struct server_vars));
	if (!ctx->svars) {
		mtcp_close(ctx->mctx, ctx->ep);
		mtcp_destroy_context(ctx->mctx);
		free(ctx);
		TRACE_ERROR("Failed to create server_vars struct!\n");
		return NULL;
	}

	return ctx;
}

int CreateListeningSocket(struct thread_context *ctx){
	int listener;
	struct mtcp_epoll_event ev;
	struct sockaddr_in saddr;
	int ret;

	/* create socket and set it as nonblocking */
	listener = mtcp_socket(ctx->mctx, AF_INET, SOCK_STREAM, 0);
	if (listener < 0) {
		TRACE_ERROR("Failed to create listening socket!\n");
		return -1;
	}
	ret = mtcp_setsock_nonblock(ctx->mctx, listener);
	if (ret < 0) {
		TRACE_ERROR("Failed to set socket in nonblocking mode.\n");
		return -1;
	}

	/* bind to port 12345 */
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(12345);
	ret = mtcp_bind(ctx->mctx, listener, 
			(struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		TRACE_ERROR("Failed to bind to the listening socket!\n");
		return -1;
	}

	/* listen (backlog: can be configured) */
	ret = mtcp_listen(ctx->mctx, listener, backlog);
	if (ret < 0) {
		TRACE_ERROR("mtcp_listen() failed!\n");
		return -1;
	}
	
	/* wait for incoming accept events */
	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = listener;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, listener, &ev);

	return listener;
}

void * RunServerThread(void *arg){
//	int core = *(int *)arg;
	struct server_arg * args = (struct server_arg *)arg;

	int core = args->core;
	int thread_id = args->thread_id;
//    struct hikv * hi = args->hi;
//	struct hikv_arg hikv_args = args->hikv_args;

	struct thread_context *ctx;
	mctx_t mctx;
	int listener;
	int ep;
	struct mtcp_epoll_event *events;
	int nevents;
	int i, ret;
	int do_accept;

#ifdef __EVAL_READ__
    pthread_mutex_init(&read_cb_lock, NULL);
    read_cnt = read_time = 0;
    write_cnt = write_time = 0;
#endif

#ifdef __REAL_TIME__
    pthread_mutex_init(&record_lock, NULL);
    request_cnt = byte_sent = 0;

    pthread_mutex_init(&start_lock, NULL);
    start_flag = 0;

    pthread_mutex_init(&end_lock, NULL);
#endif

#ifdef __EVAL_KV__
    pthread_mutex_init(&record_lock, NULL);
    put_cnt = get_cnt = 0;

    pthread_mutex_init(&start_lock, NULL);
    start_flag = 0;

    pthread_mutex_init(&put_end_lock, NULL);
    put_end_flag = 0;

    pthread_mutex_init(&get_end_lock, NULL);
    get_end_flag = 0;

    pthread_mutex_init(&end_lock, NULL);
#endif

#ifdef __EVAL_FRAM__
	int cycle_cnt, handle_time, cycle_time;
	cycle_cnt = handle_time = cycle_time = 0;
#endif

#ifdef __EVAL_CB__
    pthread_mutex_init(&read_lock, NULL);
    get_cnt = get_time = 0;
#endif
	
	/* initialization */
	ctx = InitializeServerThread(core);
	if (!ctx) {
		TRACE_ERROR("Failed to initialize server thread.\n");
		return NULL;
	}
	mctx = ctx->mctx;
	ep = ctx->ep;

	events = (struct mtcp_epoll_event *)
			calloc(MAX_EVENTS, sizeof(struct mtcp_epoll_event));
	if (!events) {
		TRACE_ERROR("Failed to create event struct!\n");
		exit(-1);
	}

	listener = CreateListeningSocket(ctx);
	if (listener < 0) {
		TRACE_ERROR("Failed to create listening socket.\n");
		exit(-1);
	}

	while (!done[core]) {
#ifdef __EVAL_FRAM__
		struct timeval epoll_start;
		gettimeofday(&epoll_start, NULL);
#endif
		nevents = mtcp_epoll_wait(mctx, ep, events, MAX_EVENTS, -1);
		if (nevents < 0) {
			if (errno != EINTR)
				perror("mtcp_epoll_wait");
			break;
		}
#ifdef __EVAL_FRAM__
		struct timeval handle_start;
		gettimeofday(&handle_start, NULL);
#endif
		do_accept = FALSE;
		for (i = 0; i < nevents; i++) {

			if (events[i].data.sockid == listener) {
				/* if the event is for the listener, accept connection */
				do_accept = TRUE;

			} else if (events[i].events & MTCP_EPOLLERR) {
				int err;
				socklen_t len = sizeof(err);

				/* error on the connection */
				TRACE_APP("[CPU %d] Error on socket %d\n", 
						core, events[i].data.sockid);
				if (mtcp_getsockopt(mctx, events[i].data.sockid, 
						SOL_SOCKET, SO_ERROR, (void *)&err, &len) == 0) {
					if (err != ETIMEDOUT) {
						fprintf(stderr, "Error on socket %d: %s\n", 
								events[i].data.sockid, strerror(err));
					}
				} else {
					perror("mtcp_getsockopt");
				}
				CloseConnection(ctx, events[i].data.sockid, 
						&ctx->svars[events[i].data.sockid]);

			} else if (events[i].events & MTCP_EPOLLIN) {
				#ifndef ZERO_COPY
				ret = HandleReadEvent(ctx, thread_id, events[i].data.sockid, 
						&ctx->svars[events[i].data.sockid]);
				#else
				ret = ZeroCopyProcess(ctx, thread_id, events[i].data.sockid, 
						&ctx->svars[events[i].data.sockid]);
				#endif

				if (ret == 0) {
					/* connection closed by remote host */
					CloseConnection(ctx, events[i].data.sockid, 
							&ctx->svars[events[i].data.sockid]);
				} else if (ret < 0) {
					/* if not EAGAIN, it's an error */
					if (errno != EAGAIN) {
						CloseConnection(ctx, events[i].data.sockid, 
								&ctx->svars[events[i].data.sockid]);
					}
				}

			} else {
				assert(0);
			}
		}

		/* if do_accept flag is set, accept connections */
		if (do_accept) {
			while (1) {
				ret = AcceptConnection(ctx, listener);
				if (ret < 0)
					break;
			}
#ifdef __REAL_TIME__
		    pthread_mutex_lock(&start_lock);
		    if(!start_flag){
        		gettimeofday(&g_start, NULL);
		        start_flag = 1;
    		}
    		pthread_mutex_unlock(&start_lock);
#endif

#ifdef __EVAL_KV__
		    pthread_mutex_lock(&start_lock);
		    if(!start_flag){
    		    gettimeofday(&g_start, NULL);
		        start_flag = 1;
    		}
			pthread_mutex_unlock(&start_lock);
#endif
		}
#ifdef __EVAL_FRAM__
		struct timeval end;
		gettimeofday(&end, NULL);

		if(trans_start_flag){
			double epoll_start_time = (double)epoll_start.tv_sec * 1000000 + (double)epoll_start.tv_usec;
			double handle_start_time = (double)handle_start.tv_sec * 1000000 + (double)handle_start.tv_usec;
	        double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

    	    cycle_cnt++;
	    	handle_time += (int)(end_time - handle_start_time);
	    	cycle_time += (int)(end_time - epoll_start_time);
		}

		if(do_accept){
			trans_start_flag = 1;
		}
#endif
	}

#ifdef __EVAL_FRAM__
	char buff[100];
    
    sprintf(buff, "tot_cycle %.4f tot_handle %.4f accept %.4f handleRead %.4f\n", 
			((double)cycle_time)/cycle_cnt, ((double)handle_time)/cycle_cnt, 
			((double)accept_time)/accept_cnt, ((double)read_time)/read_cnt);

    FILE * fp = fopen("cycle.txt", "a+");
    fseek(fp, 0, SEEK_END);
    
    fwrite(buff, strlen(buff), 1, fp);
    fclose(fp);	
#endif

#ifdef __REAL_TIME__

    double start_time = (double)g_start.tv_sec + ((double)g_start.tv_usec/(double)1000000);
    double end_time = (double)g_end.tv_sec + ((double)g_end.tv_usec/(double)1000000);

	double elapsed = end_time - start_time;

	FILE * fp = fopen("throughput.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "rps %.4f throughput %.4f\n", 
            ((double)request_cnt)/elapsed, ((double)byte_sent)/elapsed);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

#ifdef __EVAL_READ__
    FILE * fp = fopen("read_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "read %.4f write %.4f\n", 
                ((double)read_time)/read_cnt, ((double)write_time)/write_cnt);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

#ifdef __EVAL_KV__
    double start_time = (double)g_start.tv_sec + ((double)g_start.tv_usec/(double)1000000);
    double put_end_time = (double)put_end.tv_sec + ((double)put_end.tv_usec/(double)1000000);
    double get_end_time = (double)get_end.tv_sec + ((double)get_end.tv_usec/(double)1000000);
    double end_time = (double)g_end.tv_sec + ((double)g_end.tv_usec/(double)1000000);

    double put_exe_time = put_end_time - start_time;
	double get_exe_time = get_end_time - put_end_time;
	double scan_exe_time = end_time - get_end_time;

	FILE * fp = fopen("kv_throughput.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "put_iops %.4f get_iops %.4f scan_iops %.4f\n", 
                    ((double)put_cnt)/put_exe_time, ((double)get_cnt)/get_exe_time, ((double)scan_cnt)/scan_exe_time);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

#ifdef __EVAL_CB__
    FILE * fp = fopen("callback.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "%.4f\n", ((double)get_time)/get_cnt);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

	/* destroy mtcp context: this will kill the mtcp thread */
	mtcp_destroy_context(mctx);
	pthread_exit(NULL);

	return NULL;
}

void SignalHandler(int signum){
	int i;

	for (i = 0; i < core_limit; i++) {
		if (app_thread[i] == pthread_self()) {
			//TRACE_INFO("Server thread %d got SIGINT\n", i);
			done[i] = TRUE;
		} else {
			if (!done[i]) {
				pthread_kill(app_thread[i], signum);
			}
		}
	}
}

int main(int argc, char **argv){
	int ret;
	struct mtcp_conf mcfg;
	int cores[MAX_CPUS];
	int process_cpu;
//	int i, o;

	num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	core_limit = num_cores;
	process_cpu = -1;

	char conf_name[] = "server.conf";
	conf_file = conf_name;

	if (argc < 2) {
		TRACE_CONFIG("$%s directory_to_service\n", argv[0]);
		return FALSE;
	}
#if 0
	while (-1 != (o = getopt(argc, argv, "N:f:c:b"))) {
		switch (o) {
		case 'N':
			core_limit = mystrtol(optarg, 10);
			if (core_limit > num_cores) {
				TRACE_CONFIG("CPU limit should be smaller than the "
					     "number of CPUs: %d\n", num_cores);
				return FALSE;
			}
			/** 
			 * it is important that core limit is set 
			 * before mtcp_init() is called. You can
			 * not set core_limit after mtcp_init()
			 */
			mtcp_getconf(&mcfg);
			mcfg.num_cores = core_limit;
			mtcp_setconf(&mcfg);
			break;
		case 'f':
			conf_file = optarg;
			break;
		case 'c':
			process_cpu = mystrtol(optarg, 10);
			if (process_cpu > core_limit) {
				TRACE_CONFIG("Starting CPU is way off limits!\n");
				return FALSE;
			}
			break;
		case 'b':
			backlog = mystrtol(optarg, 10);
			break;
		}
	}
#endif

    int put_test, get_test, scan_test, scan_range;
    scan_range = 4;
    put_test = get_test = NUM_KEYS;
    scan_test = NUM_KEYS - scan_range;

	struct hikv_arg * hikv_args = (struct hikv_arg *)malloc(HIKV_ARG_SIZE);

	hikv_args->pm_size = 20;
	hikv_args->num_server_thread = 1;
	hikv_args->num_backend_thread = 1;
	hikv_args->num_warm_kv = 0;
	hikv_args->num_put_kv = put_test;
	hikv_args->num_get_kv = get_test;
	hikv_args->num_delete_kv = 0;
	hikv_args->num_scan_kv = scan_test;
	hikv_args->scan_range = scan_range;
	hikv_args->seed = 1234;
	hikv_args->scan_all = 0;

	int i;
    int client_num = 1;

    for (i = 0; i < argc; i++){
        long long unsigned n;
        char junk;
        if(sscanf(argv[i], "--core_limit=%llu%c", &n, &junk) == 1){
            core_limit = n;
			if (core_limit > num_cores) {
				TRACE_CONFIG("CPU limit should be smaller than the "
					     "number of CPUs: %d\n", num_cores);
				return FALSE;
			}
			mtcp_getconf(&mcfg);
			mcfg.num_cores = core_limit;
			mtcp_setconf(&mcfg);
        }else if(sscanf(argv[i], "--process_cpu=%llu%c", &n, &junk) == 1){
            process_cpu = n;
			if (process_cpu > core_limit) {
				TRACE_CONFIG("Starting CPU is way off limits!\n");
				return FALSE;
			}
        }else if(sscanf(argv[i], "--pm_size=%llu%c", &n, &junk) == 1){
            hikv_args->pm_size = n;
        }else if(sscanf(argv[i], "--num_server_thread=%llu%c", &n, &junk) == 1){
            hikv_args->num_server_thread = n;
        }else if(sscanf(argv[i], "--num_backend_thread=%llu%c", &n, &junk) == 1){
            hikv_args->num_backend_thread = n;
        }else if(sscanf(argv[i], "--num_warm=%llu%c", &n, &junk) == 1){
            hikv_args->num_warm_kv = n;
        }else if(sscanf(argv[i], "--num_put=%llu%c", &n, &junk) == 1){
            hikv_args->num_put_kv = n;
        }else if(sscanf(argv[i], "--num_get=%llu%c", &n, &junk) == 1){
            hikv_args->num_get_kv = n;
        }else if(sscanf(argv[i], "--num_delete=%llu%c", &n, &junk) == 1){
            hikv_args->num_delete_kv = n;
        }else if(sscanf(argv[i], "--num_scan=%llu%c", &n, &junk) == 1){
            hikv_args->num_scan_kv = n;
        }else if(sscanf(argv[i], "--scan_range=%llu%c", &n, &junk) == 1){
            hikv_args->scan_range = n;
        }else if(sscanf(argv[i], "--num_scan_all=%llu%c", &n, &junk) == 1){
            hikv_args->scan_all = n;
        }else if(sscanf(argv[i], "--num_client=%llu%c", &n, &junk) == 1){
            client_num = n;
            hikv_args->num_put_kv *= n;
            hikv_args->num_get_kv *= n;      
        }else if(i > 0){
            printf("error (%s)!\n", argv[i]);
        }
    }

	size_t pm_size = hikv_args->pm_size;
    uint64_t num_server_thread = hikv_args->num_server_thread;
    uint64_t num_backend_thread = hikv_args->num_backend_thread;
    uint64_t num_warm_kv = hikv_args->num_warm_kv;
    uint64_t num_put_kv = hikv_args->num_put_kv;

	/* initialize mtcp */
	if (conf_file == NULL) {
		TRACE_CONFIG("You forgot to pass the mTCP startup config file!\n");
		exit(EXIT_FAILURE);
	}

	ret = mtcp_init(conf_file);
	if (ret) {
		TRACE_CONFIG("Failed to initialize mtcp\n");
		exit(EXIT_FAILURE);
	}

	mtcp_getconf(&mcfg);
	if (backlog > mcfg.max_concurrency) {
		TRACE_CONFIG("backlog can not be set larger than CONFIG.max_concurrency\n");
		return FALSE;
	}

	/* if backlog is not specified, set it to 4K */
	if (backlog == -1) {
		backlog = 4096;
	}
	
	/* register signal handler to mtcp */
	mtcp_register_signal(SIGINT, SignalHandler);

	TRACE_INFO("Application initialization finished.\n");

    //Initialize Key-Value storage

	char pmem[128] = "/home/pmem0/pm";
    char pmem_meta[128] = "/home/pmem0/pmMETA";
    hi = new hikv(pm_size * 1024 * 1024 * 1024, num_server_thread, num_backend_thread, num_server_thread * (num_put_kv + num_warm_kv), pmem, pmem_meta);

	for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
		cores[i] = i;
		done[i] = FALSE;
		sv_thread_arg[i].core = i;
        sv_thread_arg[i].thread_id = i;
//        sv_thread_arg[i].hi = hi;
//		memcpy(&sv_thread_arg[i].hikv_args, &hikv_args, HIKV_ARG_SIZE);
		
		if (pthread_create(&app_thread[i], 
				   NULL, RunServerThread, (void *)&sv_thread_arg[i])) {
			perror("pthread_create");
			TRACE_CONFIG("Failed to create server thread.\n");
				exit(EXIT_FAILURE);
		}
		if (process_cpu != -1)
			break;
	}
	
	for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
		pthread_join(app_thread[i], NULL);

		if (process_cpu != -1)
			break;
	}
	
	mtcp_destroy();
	return 0;
}
