#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define QUEUE_SIZE 100
#define NUM_INPUT 3
#define NUM_OUTPUT 2
#define USEC_PER_PKT 1000

// This is an oversimplified implementation without any optimizations or core assignments yet where
// only full queues are being transfered to empty queues.

typedef struct packet {
	int inq_num;
	int outq_num;
	int sequence;
} Packet;

typedef struct queue {
	Packet buffer[QUEUE_SIZE];
	int index;
	int thread_num;
	int core_num;
} Queue;

// Writes a packet to input queue every USEC_PER_PKT until queue is full.
// Setting queues index to 0 (in main) signals thread to start writing again.
void *input_thread(void *args) {
	Queue* in_queue = (Queue*)args;
	in_queue->index = 0;
	int sequence = 1;
	while (1) {
		if (in_queue->index < QUEUE_SIZE) {
			Packet pkt;
			pkt.inq_num = in_queue->thread_num;
			pkt.sequence = sequence++;
			in_queue->buffer[in_queue->index] = pkt;
			in_queue->index++;
		}
		usleep(USEC_PER_PKT);
	}
	return NULL;		
}

// Reads and prints packet from output queue every USEC_PER_PKT until queue is emtpy
// Setting queues index to 0 (in main) signals thread to start reading again
void *output_thread(void *args) {
	Queue* out_queue = (Queue*)args;
	out_queue->index = QUEUE_SIZE;
	while (1) {
		if (out_queue->index < QUEUE_SIZE) {
			Packet pkt = out_queue->buffer[out_queue->index];
			pkt.outq_num = out_queue->thread_num;
			if (pkt.sequence != 0) {
				printf("Input Queue: %d Output Queue: %d Sequence Number: %d\n", pkt.inq_num, pkt.outq_num, pkt.sequence);
				out_queue->index++;
			} else {
				out_queue->index = QUEUE_SIZE;
			}
		}
		usleep(USEC_PER_PKT);
	}
	return NULL;
}

int main() {

	int i;

	// Initialze input queues and start input threads
	Queue input_queues[NUM_INPUT];	
	pthread_t input_threads[NUM_INPUT];
	for (i = 0; i < NUM_INPUT; i++) {
		input_queues[i].thread_num = i;
		pthread_create(&input_threads[i], NULL, input_thread, &input_queues[i]);
	}

	// Initialize output queues and start output threads
	Queue output_queues[NUM_OUTPUT];
	pthread_t output_threads[NUM_OUTPUT];
	for (i = 0; i < NUM_OUTPUT; i++) {
		input_queues[i].thread_num = i;
		pthread_create(&output_threads[i], NULL, output_thread, &output_queues[i]);
	}
	
	// Loop through looking for full input queues ready to by transfered to empty output queues
	// and memcpy them when ready.
	i = 0;
	while (1) {
		int inq = i;
		int outq = i % NUM_OUTPUT;
		if (input_queues[inq].index == QUEUE_SIZE && output_queues[outq].index == QUEUE_SIZE) {
			memcpy(output_queues[outq].buffer, input_queues[inq].buffer, (QUEUE_SIZE * sizeof(Packet)));
			input_queues[inq].index = 0;
			output_queues[outq].index = 0;
			printf("Transfered input queue: %d to output queue: %d\n", inq, outq);
		}
		i++;
		if (i == NUM_INPUT) {
			i = 0;
		}
	}
}

