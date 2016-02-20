#include "dtn.h"
#include "utilities.c"
#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime.h"
#include "button-sensors.h"
#include "lib/sensors.h"
#include "hmc5883l.h"
#include <stdio.h>
#include <string.h>

#define FLASH_LED(l) {leds_on(l); clock_delay_msec(50); leds_off(l); clock_delay(50);}
#define MAX_NEIGHBORS 16
#define MAX_RETRANSMISSIONS 4

/* This is the structure of unicast ping messages. */
typedef struct {
  uint8_t type;
  uint8_t from;
}runicast_message;
/* This structure holds information about neighbors. */
typedef struct {
  struct neighbor *next;
  rimeaddr_t addr;
  uint8_t seqno;
  clock_time_t timestamp;
  dtn_summary_vector summary;
}neighbor;

static struct broadcast_conn broadcast;
static struct unicast_conn unicast;
static struct etimer et;
static clock_time_t current_time;

/* This MEMB() definition defines a memory pool from which we allocate neighbor entries. */
MEMB(neighbors_memb, neighbor, MAX_NEIGHBORS);
/* The neighbors_list is a Contiki list that holds the neighbors we have seen thus far. */
LIST(neighbors_list);
/* This MEMB() definition defines a memory pool from which we allocate message entries. */
MEMB(messages_memb, dtn_vector, MAX_MESSAGES);
/* The neighbors_list is a Contiki list that holds the messages we have seen thus far. */
LIST(messages_list);

/*---------------------------------------------------------------------------*/
/* We first declare our two processes. */
PROCESS(broadcast_process, "Broadcast process");
PROCESS(runicast_process, "Unicast process");
PROCESS(simulate_neighbor, "Simulate process");
PROCESS(cache_cleaner, "Cache cleaner");
/* The AUTOSTART_PROCESSES() definition specifices what processes to start when this module is loaded. We put both our processes there. */
AUTOSTART_PROCESSES(&broadcast_process, &runicast_process, &simulate_neighbor, &cache_cleaner);
/*---------------------------------------------------------------------------*/
/* This function is called whenever a broadcast message is received. */
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  neighbor *n;
  dtn_summary_vector *summary_vector, *tmp;
  dtn_vector *new;
  summary_vector = packetbuf_dataptr();
  int i;
  int z;
  int flag;

  for(n = list_head(neighbors_list); n != NULL; n = list_item_next(n)) {
    /* We break out of the loop if the address of the neighbor matches the address of the neighbor from which we received this broadcast message. */
    if(rimeaddr_cmp(&n->addr, from)) {
      n->timestamp = clock_time();
      break;
    }
  }
  /* If n is NULL, this neighbor was not found in our list, and we allocate a new struct neighbor from the neighbors_memb memory pool. */
  if(n == NULL) {
    n = memb_alloc(&neighbors_memb);
    /* If we could not allocate a new neighbor entry, we give up. We could have reused an old neighbor entry, but we do not do this for now. */
    if(n == NULL) {
      printf("Something went wrong when trying to create a new neighbor in the list\n");
      return;
    }
    /* Initialize the fields. */
    rimeaddr_copy(&n->addr, from);
    /* Place the neighbor on the neighbor list. */
    list_add(neighbors_list, n);
  }
  n->summary = *summary_vector;
  n->timestamp = clock_time();

  for (z = 0; z < n->summary.header.len; z++) {
    flag = 0;
    int b;
    printf("\t---RECEIVED - Src: %d.%d | Dest: %d.%d | Seg: %d --- \n",
    n->summary.message_ids[z].src.u8[0],
    n->summary.message_ids[z].src.u8[1],
    n->summary.message_ids[z].dest.u8[0],
    n->summary.message_ids[z].dest.u8[1],
    n->summary.message_ids[z].seq);
    for(tmp = list_head(messages_list); tmp != NULL; tmp = list_item_next(tmp)) {
        if ((rimeaddr_cmp(&n->summary.message_ids[z].src, &tmp->message_ids[i].src)) &&
            (rimeaddr_cmp(&n->summary.message_ids[z].dest, &tmp->message_ids[i].dest))){
          flag = 1;
          printf("Flag 1 set ...\n" );
          break;
        }
      n = list_item_next(n);
    }
    if (flag == 0) {
      if(list_length(messages_list) < 5){
        new = memb_alloc(&messages_memb);
        memcpy(&new->message, &n->summary.message_ids[z], sizeof(dtn_vector));
        list_add(messages_list, new);
      }
      else if (list_length(messages_list) >= 5){
        printf("5 reached.. removing\n");
        list_chop(messages_list);
        new = memb_alloc(&messages_memb);
        memcpy(&new->message, &n->summary.message_ids[z], sizeof(dtn_vector));
        list_add(messages_list, new);
      }
    }
    else if (flag == 1) {
      printf("Already exists...\n");
    }
  }
}
/* This is where we define what function to be called when a broadcast is received. We pass a pointer to this structure in the broadcast_open() call below. */
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/*---------------------------------------------------------------------------*/
/* This function is called for every incoming unicast packet. */
static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{

}

static void
sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  printf("runicast message sent to %d.%d, retransmissions %d\n",to->u8[0], to->u8[1], retransmissions);
}
static void
timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  printf("runicast message timed out when sending to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}
static const struct runicast_callbacks runicast_callbacks = {recv_runicast, sent_runicast, timedout_runicast};
static struct runicast_conn runicast;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();
  broadcast_open(&broadcast, 129, &broadcast_call);
  set_power(0x01);
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(runicast_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast);)
  PROCESS_BEGIN();
  runicast_open(&runicast, 144, &runicast_callbacks);
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(simulate_neighbor, ev, data)
{
  rimeaddr_t node_addr;
  rimeaddr_t dest_addr;
  dtn_summary_vector inject, msg;
  dtn_vector *m;
  dtn_header header;
  neighbor *n;
  int i;
  int b;

  PROCESS_BEGIN();
  SENSORS_ACTIVATE(button_sensor);
  SENSORS_ACTIVATE(button2_sensor);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL((ev == sensors_event && data == &button_sensor) ||
                            (ev == sensors_event && data == &button2_sensor));
      if (ev == sensors_event && data == &button_sensor) {

        rimeaddr_copy(&node_addr, &rimeaddr_null);
        node_addr.u8[0] = 128;
        node_addr.u8[1] = 1 + (random_rand() % 6); /* [1-6] */

        printf("--- Adding: %d.%d to the neighbours list ---\n", node_addr.u8[0], node_addr.u8[1]);

        header.ver = 3;
        header.type = 2;
        header.len = 3;

        for (i = 0; i < header.len; i++) {
          rimeaddr_copy(&dest_addr, &rimeaddr_null);
          dest_addr.u8[0] = 128;
          dest_addr.u8[1] = 1 + (random_rand() % 20);
          inject.message_ids[i].dest = dest_addr;
          inject.message_ids[i].src =  node_addr;
          inject.message_ids[i].seq = 111;
        }
        inject.header = header;

        packetbuf_copyfrom(&inject, sizeof(dtn_summary_vector));
        broadcast_recv(&broadcast, &node_addr);
    }

    else if (ev == sensors_event && data == &button2_sensor){
      if(list_length(neighbors_list) > 0) {
        n = list_head(neighbors_list);
        current_time = clock_time();

        for(i = 0; i < list_length(neighbors_list); i++) {
          printf("*** ID: %d.%d, Received at: %d seconds ***\n", n->addr.u8[0], n->addr.u8[1],
            convert_time(n->timestamp));

          for (b = 0; b < n->summary.header.len; b++){
            printf("\t---NEIGHBOUR PRINTOUT - Src: %d.%d | Dest: %d.%d | Seg: %d --- \n",

            n->summary.message_ids[b].src.u8[0],
            n->summary.message_ids[b].src.u8[1],
            n->summary.message_ids[b].dest.u8[0],
            n->summary.message_ids[b].dest.u8[1],
            n->summary.message_ids[b].seq);
          }
          n = list_item_next(n);
        }

        m = list_head(messages_list);
        printf("PRINTING MESSAGES LIST");
        for(i = 0; i < list_length(messages_list); i++) {
            printf("\t --- MSGS PRINTOUT - Src: %d.%d | Dest: %d.%d | Seg: %d --- \n",

            m->message[i].hdr.message_id.src.u8[0],
            m->message[i].hdr.message_id.src.u8[1],
            m->message[i].hdr.message_id.dest.u8[0],
            m->message[i].hdr.message_id.dest.u8[1],
            m->message[i].hdr.message_id.seq);
        }
        m = list_item_next(m);
      }
    }
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cache_cleaner, ev, data)
{
  PROCESS_BEGIN();
  neighbor *n;
  dtn_summary_vector msg;
  int i;
  while(1) {
    etimer_set(&et, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    n = list_head(neighbors_list);

      for(i = 0; i < list_length(neighbors_list); i++) {

        if (convert_time(n->timestamp) < (convert_time(clock_time()) - 10)){
          printf("Popping: %d.%d - Timestamp: %d Time: %d\n", n->addr.u8[0], n->addr.u8[1], convert_time(n->timestamp), convert_time(clock_time()));
          list_remove(neighbors_list, n);
          memb_free(&neighbors_memb, n);
        }
        n = list_item_next(n);
      }
  }
PROCESS_END();
}
