/*
 *Delay Tolerant Network: protocol header
 */
#include "contiki.h"
#include "net/rime.h"
#include <stdint.h>
///Specify the number of messages we can hold and the message size
#define MAX_MESSAGES 5
#define MAX_MSG_VECTORS MAX_MESSAGES
#define MAX_MSG_SIZE 5
///Enumerate message types
enum
{
	DTN_RESERVED = 0,
	DTN_SUMMARY_VECTOR = 1,
	DTN_MESSAGE = 2,
	DTN_MESSAGE_DELIVERY = 3
};
///Holds the size of each value in the packet header
typedef struct
{
  uint8_t ver  : 3;
  uint8_t type : 2;
  uint8_t len  : 3;
}dtn_header;
/*
 *Specify the atrtibutes sent in the summary
 *vector during hte boradcast message
 */
typedef struct
{
	rimeaddr_t dest;
	rimeaddr_t src;
	uint8_t seq;
}dtn_msg_id;
/*
 *Define the message header fields,
 *message_id holds the struct dtn_msg_id
 *containing summary vector information.
 *This is included in runicast messages.
 */
typedef struct
{
	uint32_t timestamp;
	uint8_t number_of_copies;
	uint8_t length;
	dtn_msg_id  message_id;
	uint8_t reserved;
}dtn_msg_header;
/*
 *The is strcutre sent and received in broadcast
 *messages telling neighbours which messages we/they
 *have.
 */
typedef struct
{
	dtn_header header;
	dtn_msg_id message_ids[MAX_MSG_VECTORS];
}dtn_summary_vector;
/*
 *This struct contains the actual message_ids
 *sent with the mesage header
 */
typedef struct
{
	dtn_msg_header hdr;
	char msg[MAX_MSG_SIZE];
}dtn_message;
/*
 *This is sent in a runicast message to a
 * neighbour and received in a runicast message_ids
 */
typedef struct
{
	dtn_header header;
	dtn_message message[MAX_MESSAGES];
}dtn_vector;
/*
 *This was created with a next pointer
 *for iteration purposes
 */
typedef struct
{
	struct dtn_vector_list *next;
	dtn_message message;
}dtn_vector_list;
