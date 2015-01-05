#include "mpi.h"
#include <pthread.h>
#include <iostream>
#include <chrono>
#include <getopt.h>
#include <unistd.h>
#include <cstdlib>

#define DEBUG 							true
#define MSG_COMMIT_REQUEST				100
#define MSG_ABORT 						101
#define MSG_AGREED						102
#define MSG_ACK							103
#define MSG_COMMIT 						104
#define MSG_PREPARE						105
#define MSG_SIZE 						1
#define COOR_NODE						0
#define TIMEOUT							10.0

using namespace std;

enum class States {query, wait, abort, prepare, commit};

static int coord_timeout_q = 0;
static int coord_timeout_w = 0;
static int coord_timeout_p = 0;

static int coord_failure_q = 0;
static int coord_failure_w = 0;
static int coord_failure_p = 0;

static int cohort_member_timeout_q = 0;
static int cohort_member_timeout_w = 0;
static int cohort_member_timeout_p = 0;

static int cohort_member_failure_q = 0;
static int cohort_member_failure_w = 0;
static int cohort_member_failure_p = 0;

static int cohort_member_abort_q = 0;

static struct option long_options[] =
{
    {"coord_timeout_q", no_argument, &coord_timeout_q, 1},
    {"coord_timeout_w", no_argument, &coord_timeout_w, 1},
    {"coord_timeout_p", no_argument, &coord_timeout_p, 1},
    {"coord_failure_q", no_argument, &coord_failure_q, 1},
    {"coord_failure_w", no_argument, &coord_failure_w, 1},
    {"coord_failure_p", no_argument, &coord_failure_p, 1},
    {"cohort_member_timeout_q", no_argument, &cohort_member_timeout_q, 1},
    {"cohort_member_timeout_w", no_argument, &cohort_member_timeout_w, 1},
    {"cohort_member_timeout_p", no_argument, &cohort_member_timeout_p, 1},
    {"cohort_member_failure_q", no_argument, &cohort_member_failure_q, 1},
    {"cohort_member_failure_w", no_argument, &cohort_member_failure_w, 1},
    {"cohort_member_failure_p", no_argument, &cohort_member_failure_p, 1},
    {"cohort_member_abort_q", no_argument, &cohort_member_abort_q, 1},
    {0,0,0,0}
};

int node_id, size, tag;
States node_state = States::query;
auto transaction_start_time = chrono::steady_clock::now();
double current_time;

void print_debug_message (const char* message);
void check_opt (int argc, char **argv);
void coordinator ();
void cohort_member ();
void broadcast_commit_request ();
void broadcast_abort_msg ();
void broadcast_prepare_msg ();
void broadcast_commit_msg ();
void wait_for_ack_msg ();
void simulate_coordinator_failure ();
void simulate_cohort_member_failure ();
void simulate_coordinator_timeout ();
void simulate_cohort_member_timeout ();
void wait_for_commit_request_msg ();
void wait_for_agreed_msg ();
void send_abort_reply ();
void send_agreed_reply ();
void send_ack_reply ();
void wait_for_prepare_msg ();
void wait_for_commit_msg ();
void bcast (void* data, int count, MPI_Datatype datatype, int root, MPI_Comm communicator, MPI_Request *request);

int main(int argc, char **argv) {
	check_opt(argc, argv);

	srand (time(NULL));

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node_id);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

  	MPI_Barrier(MPI_COMM_WORLD);

  	if (node_id == COOR_NODE) {
  		coordinator ();
  	} else {
  		cohort_member ();
  	}

  	MPI_Barrier(MPI_COMM_WORLD);

  	MPI_Finalize();
}

void bcast(void* data, int count, MPI_Datatype datatype, int root, MPI_Comm communicator, MPI_Request *request, int tag) {
	for (int i = 1; i < size; i++) {
	    MPI_Isend(data, count, datatype, i, tag, communicator, request);
	}
}

void cohort_member () {
	int r = (rand() % size) + 1;

	if (node_state != States::abort)
		wait_for_commit_request_msg ();

	if (cohort_member_timeout_q && node_state != States::abort && node_id == r)
		simulate_cohort_member_timeout ();

	if (cohort_member_failure_q && node_state != States::abort && node_id == r)
		simulate_cohort_member_failure ();

	if (cohort_member_abort_q && node_state != States::abort && node_id == r)
		send_abort_reply ();
	
	if (node_state != States::abort)
		send_agreed_reply ();

	if (cohort_member_timeout_w && node_state != States::abort && node_id == r)
		simulate_cohort_member_timeout ();

	if (cohort_member_failure_w && node_state != States::abort && node_id == r)
		simulate_cohort_member_failure ();

	if (node_state != States::abort)
		wait_for_prepare_msg ();

	if (node_state != States::abort)
		send_ack_reply ();

	if (cohort_member_timeout_p && node_state != States::abort && node_id == r)
		simulate_cohort_member_timeout ();

	if (cohort_member_failure_p && node_state != States::abort && node_id == r)
		simulate_cohort_member_failure ();

	if (node_state != States::abort && node_state != States::commit)
		wait_for_commit_msg ();
}

void wait_for_prepare_msg () {
	bool received = false;

	while (true) {
		int data, flag;
		MPI_Status status;

		current_time = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - transaction_start_time).count() / 1000.0;

		if (current_time < TIMEOUT) {

			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
			
			if (flag) {
				MPI_Recv(&data, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

				if (status.MPI_TAG == MSG_ABORT) {
					print_debug_message("Abort msg received");
					node_state = States::abort;
					break;
				} else if (status.MPI_TAG == MSG_PREPARE) {
					print_debug_message("Prepare msg received");
					break;
				}
			}
		} else {
			print_debug_message("Transaction timeout - abort");
			node_state = States::abort;
			break;
		}
	}	
}

void wait_for_commit_msg () {
	bool received = false;

	while (true) {
		int data, flag;
		MPI_Status status;

		current_time = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - transaction_start_time).count() / 1000.0;

		if (current_time < TIMEOUT) {

			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
			
			if (flag) {
				MPI_Recv(&data, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

				if (status.MPI_TAG == MSG_ABORT) {
					print_debug_message("Abort msg received");
					node_state = States::abort;
					break;
				} else if (status.MPI_TAG == MSG_COMMIT) {
					print_debug_message("Commit msg received");
					node_state = States::commit;
					break;
				}
			}
		} else {
			print_debug_message("Transaction timeout - commit");
			node_state = States::commit;
			break;
		}
	}	
}

void send_ack_reply () {
	tag = MSG_ACK;
	MPI_Request request;

	MPI_Isend(&tag, MSG_SIZE, MPI_INT, COOR_NODE, MSG_ACK, MPI_COMM_WORLD, &request);
	print_debug_message("Ack msg sent to coordinator");

	node_state = States::prepare;
}

void send_agreed_reply () {
	tag = MSG_AGREED;
	MPI_Request request;

	MPI_Isend(&tag, MSG_SIZE, MPI_INT, COOR_NODE, MSG_AGREED, MPI_COMM_WORLD, &request);
	print_debug_message("Agreed msg sent to coordinator");

	node_state = States::wait;
}

void send_abort_reply () {
	tag = MSG_ABORT;
	MPI_Request request;

	MPI_Isend(&tag, MSG_SIZE, MPI_INT, COOR_NODE, MSG_ABORT, MPI_COMM_WORLD, &request);
	print_debug_message("Abort msg sent to coordinator");

	node_state = States::abort;
}

void simulate_cohort_member_failure () {
	print_debug_message ("Simulating cohort member failure");

	sleep(TIMEOUT + 5);

	if (node_state == States::query || node_state == States::wait) {
		print_debug_message ("The cohort member performing the failure transition - abort");
		node_state = States::abort;
	} else {
		print_debug_message ("The cohort member performing the failure transition - commit");
		node_state = States::commit;
	}
}

void simulate_cohort_member_timeout () {
	print_debug_message ("Simulating cohort member timeout");

	sleep(TIMEOUT);

	if (node_state == States::query || node_state == States::wait) {
		node_state = States::abort;
	} else {
		node_state = States::commit;
	}
}

void wait_for_commit_request_msg () {
	while (true) {
		int data, flag;
		MPI_Status status;

		current_time = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - transaction_start_time).count() / 1000.0;

		if (current_time < TIMEOUT) {

			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
			
			if (flag) {
				MPI_Recv(&data, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

				if (status.MPI_TAG == MSG_ABORT) {
					print_debug_message("Abort msg received");
					node_state = States::abort;
					break;
				} else if (status.MPI_TAG == MSG_COMMIT_REQUEST) {
					print_debug_message("Commit request msg received");
					break;
				}
			}
		} else {
			print_debug_message("Transaction timeout");
			node_state = States::abort;
			break;
		}
	}	
}

void coordinator () {
	if (coord_timeout_q && node_state != States::abort)
		simulate_coordinator_timeout ();

	if (coord_failure_q && node_state != States::abort)
		simulate_coordinator_failure ();

	if (node_state != States::abort)
		broadcast_commit_request ();

	if (node_state != States::abort)
		wait_for_agreed_msg ();

	if (coord_timeout_w && node_state != States::abort)
		simulate_coordinator_timeout ();

	if (coord_failure_w && node_state != States::abort)
		simulate_coordinator_failure ();

	if (node_state != States::abort)
		broadcast_prepare_msg ();

	if (node_state != States::abort)
		wait_for_ack_msg ();

	if (coord_timeout_p && node_state != States::abort)
		simulate_coordinator_timeout ();

	if (coord_failure_p && node_state != States::abort)
		simulate_coordinator_failure ();

	if (node_state != States::abort && node_state != States::commit)
		broadcast_commit_msg ();
}

void simulate_coordinator_failure () {
	print_debug_message ("Simulating coordinator failure");

	sleep(TIMEOUT + 5);

	if (node_state == States::query || node_state == States::wait) {
		print_debug_message ("The coordinator performing the failure transition - abort");
		broadcast_abort_msg ();
	} else {
		print_debug_message ("The coordinator performing the failure transition - commit");
		node_state = States::commit;
	}
}

void simulate_coordinator_timeout () {
	print_debug_message ("Simulating coordinator timeout");

	sleep(TIMEOUT);

	broadcast_abort_msg ();
}

void print_debug_message (const char* message) {
	if (DEBUG)
		cout << "Node [" << node_id << "]: " << message << endl;
}

void check_opt (int argc, char **argv) {
	char c;

	while (true) {
		int option_index = 0;

	  	c = getopt_long (argc, argv, ":",
	                   	 long_options, &option_index);

	  if (c == -1)
	    break;
	}
}

void wait_for_ack_msg () {
	int recv_count = 1;

	while (recv_count < size) {
		int data, flag;
		MPI_Status status;

		current_time = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - transaction_start_time).count() / 1000.0;

		if (current_time < TIMEOUT) {

			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
			
			if (flag) {
				MPI_Recv(&data, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

				if (status.MPI_TAG == MSG_ABORT) {
					broadcast_abort_msg ();
					break;
				} else if (status.MPI_TAG == MSG_ACK) {
					print_debug_message("Ack msg received");
					recv_count++;
				}
			}
		} else {
			print_debug_message("Transaction timeout");
			broadcast_abort_msg ();
			break;
		}
	}
}

void broadcast_commit_msg () {
	print_debug_message("All cohorts sent Ack msg");

	MPI_Request request;
	tag = MSG_COMMIT;

	bcast(&tag, MSG_SIZE, MPI_INT, node_id, MPI_COMM_WORLD, &request, tag);
	print_debug_message("Commit msg sent to all cohorts");

	node_state = States::commit;

}
void broadcast_prepare_msg () {
	print_debug_message("All cohorts agreed");

	MPI_Request request;
	tag = MSG_PREPARE;
	
	bcast(&tag, MSG_SIZE, MPI_INT, node_id, MPI_COMM_WORLD, &request, tag);
	print_debug_message("Prepare msg sent to all cohorts");

	node_state = States::prepare;
}

void broadcast_abort_msg () {
	MPI_Request request;
	tag = MSG_ABORT;

	bcast(&tag, MSG_SIZE, MPI_INT, node_id, MPI_COMM_WORLD, &request, tag);
	print_debug_message("Abort msg sent to all cohorts");

	node_state = States::abort;
}

void broadcast_commit_request () {
	tag = MSG_COMMIT_REQUEST;
	MPI_Request request;

	bcast(&tag, MSG_SIZE, MPI_INT, node_id, MPI_COMM_WORLD, &request, tag);
	print_debug_message("Commit_Request msg sent to all cohorts");

	node_state = States::wait;
}

void wait_for_agreed_msg () {
	int recv_count = 1;

	while (recv_count < size) {
		int data, flag;
		MPI_Status status;

		current_time = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - transaction_start_time).count() / 1000.0;

		if (current_time < TIMEOUT) {

			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

			if (flag) {
  				MPI_Recv(&data, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

  				if (status.MPI_TAG == MSG_ABORT) {
  					broadcast_abort_msg ();
  					break;
  				} else if (status.MPI_TAG == MSG_AGREED) {
  					print_debug_message("Agreed msg received");
  					recv_count++;
  				}
			}
		} else {
			print_debug_message("Transaction timeout");
			broadcast_abort_msg ();
			break;
		}
	}
}