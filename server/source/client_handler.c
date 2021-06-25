#include "client_handler.h"

#include "logger.h"
#include "utility.h"
#include "hashtab.h"

#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <signal.h>
#include <inttypes.h>

struct header_data {
	uint32_t id;			// 4
	uint8_t passwd[32];		// 36
	uint8_t method;			// 37
	uint8_t quality;		// 38
	uint32_t filesize;		// 42
	uint32_t checksum;		// 46

	uint8_t dummy[1024 - 46];
} __attribute__((packed));

struct event_data {
	int fd;

	struct header_data header;
	uint8_t *body;
	int bodyfd;

	size_t body_recv;
	size_t header_recv;
	size_t sz_header;
	size_t sz_body;

	struct slicer_data *slicer;
};

typedef struct header_data *HeaderData;
typedef struct event_data *EventData;
// ==========================================================
// =			Private Function		    =
// ==========================================================
// Event Data
// ==========================================================
static inline EventData event_data_create(
		int client,
		size_t timeout)
{
	struct event_data *data;

	data = malloc(sizeof(struct event_data) + sizeof(int));
	if (data == NULL)
		return NULL;

	data[1].fd = create_timer();
	if (data[1].fd < 0) {
		free(data);
		return NULL;
	}

	if (set_timer(data[1].fd, timeout) < 0) {
		close(data[1].fd);
		free(data);
	} else data[1].fd = -data[1].fd;

	data[0].fd = client;

	data[0].body = NULL;

	data[0].bodyfd = -1;
	data[0].body_recv = 0;
	data[0].sz_body = 0;

	data[0].slicer = NULL;

	data[0].header_recv = 0;
	data[0].sz_header = sizeof(struct header_data);

	return data;
}

static inline int event_data_register(
		EpollHandler handler,
		EventData data)
{
	int ret = epoll_handler_register(
			handler,
			data[0].fd,
			&data[0],
			EPOLLIN | EPOLLET);
	if (ret < 0)
		return -1;

	ret = epoll_handler_register(
			handler,
			-data[1].fd,
			&data[1],
			EPOLLIN | EPOLLET);
	if (ret < 0) {
		epoll_handler_unregister(
				handler,
				data[0].fd);
		return -2;
	}

	return 0;
}

static inline void event_data_destroy(
		struct event_data *data, EpollHandler handler)
{
	if (handler) {
		epoll_handler_unregister(handler, data[0].fd);
		epoll_handler_unregister(handler, -data[1].fd);
	}

	close(-data[1].fd);
	free(data);
}
// ==========================================================
// Etc.
// ==========================================================
static inline int wait_other_thread(sem_t *sems)
{
	int sval;

	if (sem_wait(&sems[0]) != 0) return -1;

	if (sem_wait(&sems[1]) != 0) return -2;

	if (sem_post(&sems[1]) != 0) return -5;

	if (sem_getvalue(&sems[2], &sval) != 0) return -3;

	if (sval != 1) return -4;

	return 0;
}
// ==========================================================
// Process Listener
// ==========================================================
static inline int process_listener_data(struct epoll_event *event,
					CListenerDataPtr listener,
/* NEVER use the `trace` as a static variables. */     int *trace)
{
	struct event_data *data;
	int ret, fd, client;
	int timerfd;

	fd = event->data.fd;

	for (; (client = accept(fd, NULL, NULL)) != -1;
	       (*trace)++, *trace %= listener->deliverer_count) 
	{
		if ((ret = change_nonblocking(client)) < 0) {
			pr_err("failed to change_nonblocking(): %d", ret);
			close(client);
			continue;
		}

		data = event_data_create(client, listener->timeout);
		if (data == NULL) {
			pr_err("failed to event_data_create(): %p", data);
			close(client);
			continue;
		}

		if ((ret = event_data_register(listener->deliverer[*trace],
					       data)) < 0) {
			pr_err("failed to event_data_register(): %d", ret);
			event_data_destroy(data, NULL);
			close(client);
			continue;
		}

		pr_out("foreign client connect to server "
		       "[fd:%d => deliverer:%d]", client, *trace);
	}

	if (errno != EAGAIN) {
		pr_err("failed to accept(): %s", strerror(errno));
		return -1;
	}

	return 0;
}
// ==========================================================
// Process Deliverer
// ==========================================================
static inline int check_checksum(struct header_data *data)
{
	uint32_t checksum = 0;

	checksum ^= data->id;
	checksum ^= data->passwd[0];
	checksum ^= data->filesize;
	checksum ^= data->quality;
	checksum ^= data->method;

	if (checksum != data->checksum)
		return -1;

	return 0;
}

static inline void sever_deliverer_data(
		EventData data,
		EpollHandler handler,
		int ret)
{
	int real_fd = data->fd < 0 ? data[-1].fd : data->fd;

	if (data->fd < 0)
		pr_out("client %d time-out: close session", real_fd);
	else if (ret == 0)
		pr_out("client %d request closing session", real_fd);
	else
		pr_out("failed to process_deliverer_data(%d): %d",
			real_fd, ret);

	if (data->fd < 0)
		data--;

	ftruncate(data->bodyfd, 0);
	munmap(data->body, data->sz_body);
	close(data->bodyfd);

	event_data_destroy(data, handler);
	close(real_fd);
}

static inline void show_deliverer_data(
		const char *str,
		EventData data)
{
	struct header_data *header = &data->header;

	pr_out("%s\n"
		"id: %"		PRIu32	"\n"
		"passwd: %"	"s"	"\n"
		"method: %"	PRIu8	"\n"
		"quality: %"	PRIu8	"\n"
		"filesize: %"	PRIu32	"\n"
		"checksum: %"	PRIu32,
		str,
		header->id,
		header->passwd,
		header->method,
		header->quality,
		header->filesize,
		header->checksum);
}

static inline int receive_deliverer_data(
		struct event_data *data)
{
	size_t *to_read, read_entity;
	size_t *received;
	int ret;
	uint8_t *ptr;

	to_read = &read_entity;
	if (data->bodyfd == -1) {
		received = &data->header_recv;
		*to_read = data->sz_header - *received;
		ptr = (void *) &data->header;
	} else {
		received = &data->body_recv;
		*to_read = data->sz_body - *received;
		ptr = data->body;
	}

	while (true) {
		ret = recv(data->fd, (void *) ptr + *received, *to_read, 0);
		switch (ret) {
		case -1: if (errno == EAGAIN)
				return 0;

			 pr_err("failed to recv(): %s", strerror(errno));
			 return -2;
		case  0: return -1;
		default: *received += ret;
			 *to_read -= ret;
		}

		if (*to_read == 0)
			return (data->bodyfd == -1) ? 1 : 2;
	}

	return -3;
}

static inline int allocate_body(EventData data)
{
	char fname[1024];

	sprintf(fname, "%dto%d.stl", data->fd, data->header.id);

	data->bodyfd = open(fname, O_RDWR | O_CREAT, 0664); //rw-rw-r--
	if (data->bodyfd == -1) {
		pr_err("failed to open(): %s", strerror(errno));
		return -1;
	}

	ftruncate(data->bodyfd, data->sz_body);

	data->body = mmap((void *) 0, data->sz_body,
			  PROT_READ | PROT_WRITE, MAP_SHARED,
			  data->bodyfd, 0);
	if (data->body == MAP_FAILED) {
		pr_err("failed to mmap(): %s", strerror(errno));
		close(data->bodyfd);
		return -2;
	}

	return 0;
}

static inline int process_deliverer_data(struct epoll_event *event,
		                         CDelivererDataPtr deliverer)
{
	struct event_data *data = event->data.ptr;
	int err, ret;

	if (data[0].fd < 0)
		return 0;

RECEIVE_AGAIN: ret = receive_deliverer_data(data);
	switch (ret) {
	case 1: // receive all the header data
		if ((err = check_checksum(&data->header)) < 0) {
			pr_err("failed to check_checksum(): %d", err);
			return -1;
		}

		data->sz_body = data->header.filesize;
		if ((err = allocate_body(data)) < 0) {
			pr_err("failed to allocate_body(): %d", err);
			return -2;
		}
		
		show_deliverer_data("received body data", data);

		goto RECEIVE_AGAIN;
	case 2: // receive all the body data
		pr_out("received all the data from client: %d", data->fd);
		// destroy timerfd
		int timerfd = -data[1].fd;
		epoll_handler_unregister(deliverer->epoll, timerfd);
		close(timerfd);

		// unregister epoll handler
		epoll_handler_unregister(deliverer->epoll, data[0].fd);

		pr_out("enqueu client %d data: deliverer %d > worker",
			data[0].fd, deliverer->id);

		err = queue_enqueue(deliverer->queue, data);
		if (err < 0) {
			pr_err("failed to enqueue_data: %d", err);
			return -3;
		}

		break;
	case  0: // receive_deliverer_data() =>  0 : recv(EAGAIN)
		 return 1;
	case -1: // receive_deliverer_data() => -1 : recv() => 0(close)
		 return 0;
	default: // receive_deliverer_data() => -2 : recv(ERROR)
		 // receive_deliverer_data() => -3 : unrecovrable error
		 pr_err("failed to receive_deliverer_data(): %d", ret);
		 return ret;
	}

	return ret;
}
// ==========================================================
// Process Worker
// ==========================================================
#define DEFAULT_PRINTER_SETTINGS \
	"resources/definitions/fdmprinter.def.json"
#define SLICER_LOCATION \
	"./CuraEngine"

struct slicer_data {
	int pid;
	int rstream;

	char stl[FILENAME_MAX];
	char output[FILENAME_MAX];
};

typedef struct slicer_data *Slicer;

static inline void event_data_cleanup(EventData data)
{
	close(data->fd);

	close(data->bodyfd);
	ftruncate(data->bodyfd, 0);
	munmap(data->body, data->sz_body);
}

static inline int launch_slicer(EventData data)
{
	int link[2], pid, ret = 0;
	Slicer slicer;

	slicer = malloc(sizeof(struct slicer_data));
	if (slicer == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		ret = -1; goto RETURN_RET;
	}

	sprintf(slicer->stl, "%dto%d.stl", data->fd, data->header.id);
	sprintf(slicer->output, "%dto%d.gcode", data->fd, data->header.id);

	if (pipe(link) == -1) {
		pr_err("failed to pipe(): %s", strerror(errno));
		ret = -2; goto FREE_SLICER;
	}

	if ((pid = fork()) == -1) {
		pr_err("failed to fork(): %s", strerror(errno));
		ret = -3; goto BREAK_PIPE;
	}
	
	if (pid == 0) {
		if (dup2(link[1], STDERR_FILENO) < 0) {
			pr_err("failed to dup2(): %s", strerror(errno));
			exit(EXIT_FAILURE);
		}

		close(link[0]); close(link[1]);

		execl(SLICER_LOCATION, "CuraEngine", "slice", 
		      "-j", DEFAULT_PRINTER_SETTINGS,
		      "-l", slicer->stl,
		      "-o", slicer->output,
		      NULL);
		// the case should NEVER happen.
		exit(EXIT_FAILURE);
	}

	close(link[1]); // read-only
	
	slicer->pid = pid;
	slicer->rstream = link[0];
	data->slicer = slicer;

	return ret;
BREAK_PIPE: close(link[0]); close(link[1]);
FREE_SLICER: free(slicer);
RETURN_RET: return ret;
}

static inline int send_progress_client(EventData data)
{
	char buffer[4096], *ptr;
	int err, ret;
	double dprog;
	int32_t iprog;

	err = read(data->slicer->rstream, buffer, sizeof(buffer) - 1);
	pr_out("read return: %d\n%s\n", err, buffer);
	if (err == 0 || err == -1) {
		err = waitpid_timed(data->slicer->pid, &ret, 0, 1);
		if (err != data->slicer->pid) {
			pr_err("failed to waitpid_timed(): %s", strerror(errno));
			return -5;
		} else pr_out("waitpid_timed() return: %d", err);

		if ( !WIFEXITED(ret) ) {
			pr_err("WIFEXITED(pid %d) return false",
				data->slicer->pid);
			return -6;
		} else if ((ret = WEXITSTATUS(ret)) != EXIT_SUCCESS) {
			pr_err("failed to slice the file: %d", ret);
			return -7;
		}

		iprog = 100;
		err = send(data->fd, &iprog, sizeof(int32_t), O_NONBLOCK);
		if (err == -1) {
			pr_err("failed to send(): %s", strerror(errno));
			return -3;
		}

		return 1;
	}

	ptr = last_strstr(buffer, "Progress:");
	if (ptr == NULL) return 0;

	if (sscanf(ptr, "Progress:%*[A-Za-z+]:%*d:%*d %lf", &dprog) != 1) {
		pr_err("failed to sscanf(): %s", "return value isn't 1");
		return 0;
	} else iprog = (int) (dprog * 100.0);

	err = send(data->fd, &iprog, sizeof(int32_t), O_NONBLOCK);
	if (err == -1) {
		pr_err("failed to send(): %s", strerror(errno));
		return -3;
	}

	return (iprog == 100);
}

static inline void destroy_slicer(Slicer slicer)
{
	if (slicer->pid > 0)
		kill(slicer->pid, SIGHUP);

	close(slicer->rstream);
	free(slicer);
}

static inline int send_output_device(EventData data, Hashtab table)
{
	void *find_ptr;
	int ret;
	int devfd, id;

	id = data->header.id;

	find_ptr = hashtab_find(table, (void *) (intptr_t) id, false);
	if (!find_ptr) {
		pr_err("failed to hashtab_find(%d)", id);
		return -1;
	} else devfd = (intptr_t) find_ptr;

	ret = send_timeout(devfd, &data->header, data->sz_header, 3);
	if (ret < 0) {
		pr_err("failed to send_timeout(header): %d, %s",
			ret, strerror(errno));
		hashtab_find(table, (void *) (intptr_t) id, true);
		return -2;
	}
	
	ret = send_timeout(devfd, data->body, data->sz_body, 3);
	if (ret < 0) {
		pr_err("failed to send_timed(body): %d, %s",
			ret, strerror(errno));
		hashtab_find(table, (void *) (intptr_t) id, true);
		return -3;
	}

	return 0;
}

static inline int process_worker_data(EventData data, Hashtab table)
{
	int err, ret;
	int progress = data->header_recv;

	if (data->slicer == NULL)
		progress = data->header_recv = 0;

	switch (progress) {
	case 0: err = launch_slicer(data);
		if (err < 0) {
			pr_err("failed to launch_slicer(): %d", err);
			ret = -1; goto CLEANUP;
		} else data->header_recv++;
		break;
	case 1: err = send_progress_client(data);
		if (err < 0) {
			pr_err("failed to send_progress(): %d", err);
			ret = -2; goto CLEANUP;
		} else if (err > 0) data->header_recv++;
		break;
	case 2: err = send_output_device(data, table);
		err = -1;
		if (err < 0) {
			pr_err("failed to send_output_to_device(): %d", err);
			ret = -3; goto CLEANUP;
		}
		break;
	case 3: destroy_slicer(data->slicer);
		break;
	}

	return 0;
CLEANUP:destroy_slicer(data->slicer);
	return ret;
}
// ==========================================================
// =			Public Function			    =
// ==========================================================
void *client_handler_listener(void *argument)
{
	CListenerDataPtr listener = argument;
	struct epoll_event *event;
	int ret, trace = 0;

	if ((ret = wait_other_thread(listener->sync)) < 0) {
		pr_err("failed to wait_other_thread: %d", ret);
		return (void *) -1;
	}

	pr_out("listener thread start: %d", listener->id);

	while (true) {
		ret = epoll_handler_wait(listener->listener, -1);
		if (ret < 0) {
			pr_err("failed to epoll_handler_wait(): %d", ret);
			continue;
		} 

		for (int i = 0; i < ret; i++) {
			event = epoll_handler_pop(listener->listener);

			if (event == NULL) {
				pr_err("failed to epoll_handler_pop(): %p", 
					event);
				continue;
			}

			ret = process_listener_data(
				event, listener, &trace
			);

			if (ret < 0) {
				pr_err("failed to process_listener_data(): %d",
					ret);
				continue;
			}
		}
	}

	return (void *) 0;
}

void *client_handler_deliverer(void *argument)
{
	CDelivererDataPtr deliverer = argument;
	struct epoll_event *event;
	int ret, nclient, cast_count;

	if ((ret = wait_other_thread(deliverer->sync)) < 0) {
		pr_err("failed to wait_other_thread: %d", ret);
		return (void *) -1;
	}

	pr_out("deliverer thread start: %d", deliverer->id);

	while (true) {
		nclient = epoll_handler_wait(deliverer->epoll, -1);
		if (nclient < 0) {
			pr_err("failed to epoll_handler_wait(): %d", ret);
			continue;
		}

		cast_count = 0;
		for (int i = 0; i < nclient; i++) {
			event = epoll_handler_pop(deliverer->epoll);

			if (event == NULL) {
				pr_err("failed to epoll_handler_pop(): %p", 
					event);
				continue;
			} else ret = process_deliverer_data(
				event, deliverer
			);

			// ret  0: close-request recv() => 0
			// ret -1: recv() == -1, set errno: not EAGAIN
			if (ret <= 0) sever_deliverer_data(
				event->data.ptr,
				deliverer->epoll,
				ret
			); else cast_count++; 
		}

		if (cast_count > 1) queue_broadcast(deliverer->queue);
		else if (cast_count == 1) queue_wakeup(deliverer->queue);
	}

	// int x = 0xfull * 0xcafedeep-0f / 0xdeadbeef / 0x01dB1ade;
	// full cafedeep of deadbeef OldBlade 

	return (void *) 0;
}

void *client_handler_worker(void *argument)
{
	CWorkerDataPtr worker = argument;
	EventData data;
	int ret;
	int32_t result;

	if ((ret = wait_other_thread(worker->sync)) < 0) {
		pr_err("failed to wait_other_thread: %d", ret);
		return (void *) -1;
	}

	pr_out("worker thread start: %d", worker->id);

	while (true) {
		data = queue_dequeue(worker->queue);
		if (data == NULL) {
			pr_err("failed to queue_dequeue: %p", data);
			continue;
		}

		pr_out("dequeuing data from the deliverer: %d", data->fd);

		ret = process_worker_data(data, worker->table);
		switch (ret) {
		case 0: // Processing data...
			queue_enqueue(worker->queue, data);
			queue_wakeup(worker->queue);
			break;
		default:// failed to handling
			pr_err("failed to process_worker_data(%d): %d",
				data->fd, ret);
		case 1: // processing success.
			result = ret;
			send(data->fd, &result, sizeof(int32_t), MSG_DONTWAIT);
			event_data_cleanup(data);
			break;
		}
	}

	return (void *) 0;
}

