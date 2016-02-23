/**
 *        Delay Tolerant Network: protocol header
 */

#include "contiki.h"
#include "net/rime.h"
#include <stdint.h>

#define MAX_MESSAGES 5

#define MAX_MSG_VECTORS MAX_MESSAGES
#define MAX_MSG_SIZE 5

enum
{
	DTN_RESERVED = 0,
	DTN_SUMMARY_VECTOR = 1,
	DTN_MESSAGE = 2,
	DTN_MESSAGE_DELIVERY = 3
};

typedef struct
{
  uint8_t ver  : 3;
  uint8_t type : 2;
  uint8_t len  : 1;
}dtn_header;

typedef struct
{
	rimeaddr_t dest;
	rimeaddr_t src;
	uint8_t seq;
}dtn_msg_id;

typedef struct
{
	uint32_t timestamp;
	uint8_t number_of_copies;
	uint8_t length;
	dtn_msg_id  message_id;
	uint8_t reserved;
}dtn_msg_header;

//broadcast messages
typedef struct
{
	dtn_header header;
	dtn_msg_id message_ids[MAX_MSG_VECTORS];
}dtn_summary_vector;

typedef struct
{
	dtn_msg_header hdr;
	char msg[MAX_MSG_SIZE];
}dtn_message;

//uniacst massages
typedef struct
{
	dtn_header header;
	dtn_message message[MAX_MESSAGES];
}dtn_vector;


typedef struct
{
	struct dtn_vector_list *next;
	dtn_message message;
}dtn_vector_list;
