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
MEMB(messages_memb, dtn_vector_list, MAX_MESSAGES);
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
  dtn_summary_vector *received, *new;
  dtn_vector_list *tmp;
  received = packetbuf_dataptr();
  int flag;
  int i;
  for(tmp = list_head(messages_list); tmp != NULL; tmp = list_item_next(tmp)) {
    for (i = 0; i < received->header.len; i++){
      if (rimeaddr_cmp(&received->message_ids[i].src, &tmp->message.hdr.message_id.src) &&
          rimeaddr_cmp(&received->message_ids[i].dest, &tmp->message.hdr.message_id.dest) &&
          received->message_ids[i].seq == &tmp->message.hdr.message_id.seq) {

          printf("\t---NEED TO SEND - Src: %d.%d | Dest: %d.%d | Seq: %d --- \n",
                &tmp->message.hdr.message_id.src.u8[0],
                &tmp->message.hdr.message_id.src.u8[1],
                &tmp->message.hdr.message_id.dest.u8[0],
                &tmp->message.hdr.message_id.dest.u8[1],
                &tmp->message.hdr.message_id.seq
                );
        }
        else {

          printf("\t---DONT NEED TO SEND - Src: %d.%d | Dest: %d.%d | Seq: %d --- \n",
                &tmp->message.hdr.message_id.src.u8[0],
                &tmp->message.hdr.message_id.src.u8[1],
                &tmp->message.hdr.message_id.dest.u8[0],
                &tmp->message.hdr.message_id.dest.u8[1],
                &tmp->message.hdr.message_id.seq
                );
        }
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
    // dtn_vector_list *vector_local, *tmp, *r, *new;
    // dtn_vector *vector_rec;
    // int i;
    // int flag;
    // vector_rec = packetbuf_dataptr();
    // printf("Unicast recieved from %d.%d \n", from->u8[0], from->u8[1]);
    // for (i = 0; i < vector_rec->header.len; i++){
    //   flag = 0;
    //   for(tmp = list_head(messages_list); tmp != NULL; tmp = list_item_next(tmp)){
    //     if ((rimeaddr_cmp(&vector_rec->message[i].hdr.message_id.src, &tmp->message.hdr.message_id.src))
    //      && (rimeaddr_cmp(&vector_rec->message[i].hdr.message_id.dest, &tmp->message.hdr.message_id.dest))
    //      && (&vector_rec->message[i].hdr.message_id.seq == &tmp->message.hdr.message_id.seq)) {
    //       flag = 1;
    //       printf("Flag 1 set ...\n" );
    //       break;
    //     }
    //   }
    //   if (flag == 1){
    //     printf("Source: %d:%d | Dest: %d:%d | Seq: %d | Msg: %s - already exists...\n",
    //     vector_rec->message[i].hdr.message_id.src.u8[0], vector_rec->message[i].hdr.message_id.src.u8[1],
    //     vector_rec->message[i].hdr.message_id.dest.u8[0], vector_rec->message[i].hdr.message_id.dest.u8[1],
    //     vector_rec->message[i].hdr.message_id.seq,
    //     vector_rec->message[i].msg);
    //   }
    //   else if (flag == 0){
    //     printf("Soure: %d:%d | Dest: %d:%d | Seq: %d | Msg: %s - is being added to the message_list...\n",
    //     vector_rec->message[i].hdr.message_id.src.u8[0], vector_rec->message[i].hdr.message_id.src.u8[1],
    //     vector_rec->message[i].hdr.message_id.dest.u8[0], vector_rec->message[i].hdr.message_id.dest.u8[1],
    //     vector_rec->message[i].hdr.message_id.seq,
    //     vector_rec->message[i].msg);
    //     if(list_length(messages_list) < 5){
    //       new = memb_alloc(&messages_memb);
    //       memcpy(&new->message, &vector_rec->message[i], sizeof(dtn_vector_list));
    //       printf("%d\n", new->message.hdr.message_id.src.u8[1]);
    //       list_add(messages_list, new);
    //     }
    //     else if (list_length(messages_list) >= 5){
    //       printf("5 reached.. removing\n");
    //       //r = list_head(messages_list);
    //       list_chop(messages_list);
    //       //memb_free(&messages_memb, r);
    //       new = memb_alloc(&messages_memb);
    //       memcpy(&new->message, &vector_rec->message[i], sizeof(dtn_vector_list));
    //       list_add(messages_list, new);
    //     }
    //   }
    // }
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
    static struct etimer et;
    int i;
    dtn_summary_vector *send;
    dtn_vector_list *tmp;

    header.ver = 3;
    header.type = 2;
    header.len = 3;

    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
    PROCESS_BEGIN();
    node_addr.u8[0] = 128;
    node_addr.u8[1] = 11;
    rimeaddr_set_node_addr(&node_addr);

    broadcast_open(&broadcast, 129, &broadcast_call);

    while(1) {
        etimer_set(&et, CLOCK_SECOND * 3 + random_rand() % (CLOCK_SECOND * 3));
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        printf("*** Broadcasting my summary vector ****");
        for(tmp = list_head(messages_list); tmp != NULL; tmp = list_item_next(tmp)) {
            printf("\tItem in messages_list --Src: %d.%d | Dest: %d.%d | Seq: %d --- \n",
            &tmp->message.hdr.message_id.src.u8[0],
            &tmp->message.hdr.message_id.src.u8[1],
            &tmp->message.hdr.message_id.dest.u8[0],
            &tmp->message.hdr.message_id.dest.u8[1],
            &tmp->message.hdr.message_id.seq
            );

            send->message_ids.src = &tmp->message.hdr.message_id.src;
            send->message_ids.dest = &tmp->message.hdr.message_id.dest;
            send->message_ids.seq = &tmp->message.hdr.message_id.seq;

        }

        send->header = header;
        packetbuf_copyfrom(&send, sizeof(dtn_summary_vector));
        broadcast_send(&broadcast);
    }
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
        node_addr.u8[1] = 1 + (random_rand() % 3);

        printf("--- Adding: %d.%d to the neighbours list ---\n", node_addr.u8[0], node_addr.u8[1]);

        header.ver = 3;
        header.type = 2;
        header.len = 3;

        for (i = 0; i < header.len; i++) {
          rimeaddr_copy(&dest_addr, &rimeaddr_null);
          dest_addr.u8[0] = 128;
          dest_addr.u8[1] = 1 + (random_rand() % 5);
          inject.message_ids[i].dest = dest_addr;
          inject.message_ids[i].src =  node_addr;
          inject.message_ids[i].seq = 111;
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
        printf("PRINTING MESSAGES LIST \n");
        for(i = 0; i < list_length(messages_list); i++) {
            printf("\t --- MSGS PRINTOUT - Src: %d.%d | Dest: %d.%d | Seg: %d | Msg: %s --- \n",

            m->message[i].hdr.message_id.src.u8[0],
            m->message[i].hdr.message_id.src.u8[1],
            m->message[i].hdr.message_id.dest.u8[0],
            m->message[i].hdr.message_id.dest.u8[1],
            m->message[i].hdr.message_id.seq,
            m->message[i].msg);
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
