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
#define MAX_RETRANSMISSIONS 4

static struct broadcast_conn broadcast;
static struct runicast_conn runicast;
static struct etimer et;
static clock_time_t current_time;
/* This MEMB() definition defines a memory pool from which we allocate message entries. */
MEMB(messages_memb, dtn_vector_list, MAX_MESSAGES);
/* The neighbors_list is a Contiki list that holds the messages we have seen thus far. */
LIST(messages_list);
/*---------------------------------------------------------------------------*/
PROCESS(broadcast_process, "Broadcast process");
PROCESS(simulate_neighbor, "Simulate process");
/* The AUTOSTART_PROCESSES() definition specifices what processes to start when this module is loaded. We put both our processes there. */
AUTOSTART_PROCESSES(&broadcast_process, &simulate_neighbor);
/*---------------------------------------------------------------------------*/
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  dtn_summary_vector *broadcast_received;
  dtn_vector_list *tmp;
  static dtn_message unicast_message;
  broadcast_received = packetbuf_dataptr();
  int flag, i;
  for(tmp = list_head(messages_list); tmp != NULL; tmp = list_item_next(tmp)) {
    flag = 0;
    for (i = 0; i < broadcast_received->header.len; i++) {
      printf("--- [R-BC] Src: %d.%d | Dest: %d.%d | Seq: %d *** FROM: %d.%d --- \n",
        broadcast_received->message_ids[i].src.u8[0], broadcast_received->message_ids[i].src.u8[1],
        broadcast_received->message_ids[i].dest.u8[0], broadcast_received->message_ids[i].dest.u8[1],
        broadcast_received->message_ids[i].seq,
        from->u8[0], from->u8[1]
        // tmp->message.hdr.message_id.src.u8[0], tmp->message.hdr.message_id.src.u8[1],
        // tmp->message.hdr.message_id.dest.u8[0], tmp->message.hdr.message_id.dest.u8[1],
        // tmp->message.hdr.message_id.seq
        );
      if ((rimeaddr_cmp(&broadcast_received->message_ids[i].src, &tmp->message.hdr.message_id.src) &&
        rimeaddr_cmp(&broadcast_received->message_ids[i].dest, &tmp->message.hdr.message_id.dest) &&
        broadcast_received->message_ids[i].seq == tmp->message.hdr.message_id.seq))  {
        flag = 1;
        //printf("Flag 1 set ...\n" );
        break;
      }
      else {
        flag = 0;
        break;
      }
    }
    if (flag == 0) {
      printf("--- [S-UC] Src: %d.%d | Dest: %d.%d | Seq: %d *** TO: %d.%d --- \n",
        tmp->message.hdr.message_id.src.u8[0], tmp->message.hdr.message_id.src.u8[1],
        tmp->message.hdr.message_id.dest.u8[0], tmp->message.hdr.message_id.dest.u8[1],
        tmp->message.hdr.message_id.seq,
        from->u8[0], from->u8[1]
        );
        unicast_message.hdr.timestamp = tmp->message.hdr.timestamp;
        unicast_message.hdr.number_of_copies = tmp->message.hdr.number_of_copies;
        unicast_message.hdr.length = 1;
        unicast_message.hdr.message_id = tmp->message.hdr.message_id;

        //unicast_message.msg = tmp->message.msg;
        rimeaddr_t destination;
        rimeaddr_copy(&destination, from);

        packetbuf_copyfrom(&unicast_message, sizeof(dtn_message));
        runicast_send(&runicast, from, MAX_RETRANSMISSIONS);
    }
    else if (flag == 1) {
      printf("--- [ALERT] %d.%d already has: Src: %d.%d | Dest: %d.%d | Seq: %d | Msg: %s --- \n",
        from->u8[0], from->u8[1],
        tmp->message.hdr.message_id.src.u8[0], tmp->message.hdr.message_id.src.u8[1],
        tmp->message.hdr.message_id.dest.u8[0], tmp->message.hdr.message_id.dest.u8[1],
        tmp->message.hdr.message_id.seq,
        tmp->message.msg
        );
    }
  }
}
/* This is where we define what function to be called when a broadcast is received. We pass a pointer to this structure in the broadcast_open() call below. */
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/
/* This function is called for every incoming unicast packet. */
static void recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
  dtn_vector *unicast_recieved;
  dtn_vector_list *add_to_list, *tmp;
  int i;
  unicast_recieved = packetbuf_dataptr();
  for (i = 0; i < unicast_recieved->header.len; i++) {
    printf("--- [R-UC] Src: %d.%d | Dest: %d.%d | Copies: %d | Timestamp: %d ---\n",
      unicast_recieved->message[i].hdr.message_id.src.u8[0], unicast_recieved->message[i].hdr.message_id.src.u8[1],
      unicast_recieved->message[i].hdr.message_id.dest.u8[0], unicast_recieved->message[i].hdr.message_id.dest.u8[1],
      unicast_recieved->message[i].hdr.number_of_copies, convert_time(unicast_recieved->message[i].hdr.timestamp));
      if (unicast_recieved->message[i].hdr.message_id.dest.u8[1] == 11) {
        printf(" ********** Final desination reached **********\t --- Src: %d.%d | Dest: %d.%d | Copies: %d | Timestamp: %d ---\n",
        unicast_recieved->message[i].hdr.message_id.src.u8[0], unicast_recieved->message[i].hdr.message_id.src.u8[1],
        unicast_recieved->message[i].hdr.message_id.dest.u8[0], unicast_recieved->message[i].hdr.message_id.dest.u8[1],
        unicast_recieved->message[i].hdr.number_of_copies, convert_time(unicast_recieved->message[i].hdr.timestamp)
        );
      }
      else {
        if(list_length(messages_list) < 5){
          add_to_list = memb_alloc(&messages_memb);
          memcpy(&add_to_list->message, &unicast_recieved->message[i], sizeof(dtn_vector_list));
          list_add(messages_list, add_to_list);
        }
        else if (list_length(messages_list) >= 5){
          printf("--- [ALERT] Popping last element\n");
          tmp = list_pop(messages_list);
          memb_free(&messages_memb, tmp);
          add_to_list = memb_alloc(&messages_memb);
          memcpy(&add_to_list->message, &unicast_recieved->message[i], sizeof(dtn_vector_list));
          list_add(messages_list, add_to_list);
        }
      }
    }
}

static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  printf("--- [ALERT] Runicast sent to %d.%d, retransmissions %d\n",to->u8[0], to->u8[1], retransmissions);
}
static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  printf("--- [ALERT] Runicast message timed out when sending to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}
static const struct runicast_callbacks runicast_callbacks = {recv_runicast, sent_runicast, timedout_runicast};
static struct runicast_conn runicast;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data)
{
  static struct etimer et;
  int i, h;
  static dtn_summary_vector send;
  dtn_vector_list *my_vector;
  dtn_header header;
  rimeaddr_t node_addr;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();
  set_power(10);
  node_addr.u8[0] = 128;
  node_addr.u8[1] = 11;
  rimeaddr_set_node_addr(&node_addr);

  broadcast_open(&broadcast, 129, &broadcast_call);
  runicast_open(&runicast, 144, &runicast_callbacks);

  while(1) {
      etimer_set(&et, CLOCK_SECOND * 5 + random_rand() % (CLOCK_SECOND * 5));
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      i = 0;
      for(my_vector = list_head(messages_list); my_vector != NULL; my_vector = list_item_next(my_vector)) {
        // printf("--- [ALERT] \t [BC VECTOR ITEM]: Src: %d.%d | Dest: %d.%d | Seq: %d --- \n",
        // my_vector->message.hdr.message_id.src.u8[0], my_vector->message.hdr.message_id.src.u8[1],
        // my_vector->message.hdr.message_id.dest.u8[0], my_vector->message.hdr.message_id.dest.u8[1],
        // my_vector->message.hdr.message_id.seq);
        // send->message_ids[0].src = &tmp->message.hdr.message_id.src;
        // send->message_ids[0].dest = &tmp->message.hdr.message_id.dest;
        // send->message_ids[0].seq = &tmp->message.hdr.message_id.seq;
        send.message_ids[i++] = (my_vector->message.hdr.message_id);
      }
      header.type = DTN_SUMMARY_VECTOR;
      header.len = i;
      send.header = header;
      send.message_ids;
      packetbuf_copyfrom(&send, sizeof(dtn_summary_vector));
      broadcast_send(&broadcast);
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(simulate_neighbor, ev, data)
{
  rimeaddr_t node_addr, dest_addr;
  static dtn_summary_vector sim_broadcast;
  static dtn_vector sim_unicast;
  dtn_vector_list *m;
  dtn_header header;
  int i, b;
  PROCESS_BEGIN();
  SENSORS_ACTIVATE(button_sensor);
  SENSORS_ACTIVATE(button2_sensor);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL((ev == sensors_event && data == &button_sensor) ||
                            (ev == sensors_event && data == &button2_sensor));
      if (ev == sensors_event && data == &button_sensor) {
        rimeaddr_copy(&node_addr, &rimeaddr_null);
        node_addr.u8[0] = 128;
        node_addr.u8[1] = 1 + (random_rand() % 3);
        header.ver = 3;
        header.type = 2;
        header.len = 1;
        for (i = 0; i < header.len; i++) {
          rimeaddr_copy(&dest_addr, &rimeaddr_null);
          dest_addr.u8[0] = 128;
          dest_addr.u8[1] = 1 + (random_rand() % 5);
          sim_broadcast.message_ids[i].dest = dest_addr;
          sim_broadcast.message_ids[i].src =  node_addr;
          sim_broadcast.message_ids[i].seq = 1;
        }
        sim_broadcast.header = header;
        packetbuf_copyfrom(&sim_broadcast, sizeof(dtn_summary_vector));
        //broadcast_recv(&broadcast, &node_addr);
        /* Create an example variable capable of holding 50 characters */
        for (i = 0; i < header.len; i++) {
          rimeaddr_copy(&dest_addr, &rimeaddr_null);
          dest_addr.u8[0] = 128;
          dest_addr.u8[1] = 1 + (random_rand() % 5);
          sim_unicast.message[i].hdr.message_id.dest = dest_addr;
          sim_unicast.message[i].hdr.message_id.src =  node_addr;
          sim_unicast.message[i].hdr.message_id.seq = 1;
          sim_unicast.message[i].hdr.number_of_copies =  10;
          sim_unicast.message[i].hdr.timestamp =  clock_time();
          // char example[6];
          // strcpy (example, "Hello");
          // sim_unicast.message[i].msg = "Hi";
        }
        sim_unicast.header = header;
        packetbuf_copyfrom(&sim_unicast, sizeof(dtn_vector));
        recv_runicast(&runicast, &dest_addr, MAX_RETRANSMISSIONS);
    }
    else if (ev == sensors_event && data == &button2_sensor){
      if(list_length(messages_list) > 0) {
        m = list_head(messages_list);
        for(m = list_head(messages_list); m != NULL; m = list_item_next(m)) {
          printf("--- [ALERT][M LIST ITEM]: Src: %d.%d | Dest: %d.%d | Seg: %d --- \n",
          m->message.hdr.message_id.src.u8[0], m->message.hdr.message_id.src.u8[1],
          m->message.hdr.message_id.dest.u8[0], m->message.hdr.message_id.dest.u8[1],
          m->message.hdr.message_id.seq);
          // m->message.msg);
        }
      }
      else {
        printf("--- [ALERT][M LIST]: Empty\n");
      }
    }
  }
  PROCESS_END();
}
