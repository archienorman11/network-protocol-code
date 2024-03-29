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
struct runicast_message {
  uint8_t type;
  uint8_t from;
};
/* This structure holds information about neighbors. */
struct neighbor {
  struct neighbor *next;
  rimeaddr_t addr;
  uint8_t seqno;
  clock_time_t timestamp;
  dtn_summary_vector summary;
};

static struct broadcast_conn broadcast;
static struct unicast_conn unicast;
static struct etimer et;
static clock_time_t current_time;

/* This MEMB() definition defines a memory pool from which we allocate neighbor entries. */
MEMB(neighbors_memb, struct neighbor, MAX_NEIGHBORS);
/* The neighbors_list is a Contiki list that holds the neighbors we have seen thus far. */
LIST(neighbors_list);
/* This MEMB() definition defines a memory pool from which we allocate message entries. */
MEMB(messages_memb, dtn_summary_vector, MAX_MESSAGES);
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
  struct neighbor *n;
  dtn_summary_vector *summary_vector;
  summary_vector = packetbuf_dataptr();
  int i;
  for (i = 0; i < summary_vector->header.len; i++){
     printf("***From: %d.%d - %d.%d|%d.%d|%d***\n",
     from->u8[0],from->u8[1],
     summary_vector->message_ids[i].src.u8[0], summary_vector->message_ids[i].src.u8[1],
     summary_vector->message_ids[i].dest.u8[0], summary_vector->message_ids[i].dest.u8[1],
     summary_vector->message_ids[i].seq);
  }
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
    n->timestamp = clock_time();
    /* Place the neighbor on the neighbor list. */
    list_add(neighbors_list, n);
    //Parse received broadcast
  }
  n->summary = *summary_vector;
}
/* This is where we define what function to be called when a broadcast is received. We pass a pointer to this structure in the broadcast_open() call below. */
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};


/*---------------------------------------------------------------------------*/
/* This function is called for every incoming unicast packet. */
static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
    dtn_vector_list *vector_local, *tmp, *r, *new;
    dtn_vector *vector_rec;
    int i;
    int flag;
    vector_rec = packetbuf_dataptr();
    printf("Unicast recieved from %d.%d \n", from->u8[0], from->u8[1]);
    for (i = 0; i < vector_rec->header.len; i++){
      flag = 0;
      for(tmp = list_head(messages_list); tmp != NULL; tmp = list_item_next(tmp)){
        if ((rimeaddr_cmp(&vector_rec->message[i].hdr.message_id.src, &tmp->message.hdr.message_id.src))
         && (rimeaddr_cmp(&vector_rec->message[i].hdr.message_id.dest, &tmp->message.hdr.message_id.dest))
         && (&vector_rec->message[i].hdr.message_id.seq == &tmp->message.hdr.message_id.seq)) {
          flag = 1;
          printf("Flag 1 set ...\n" );
          break;
        }
      }
      if (flag == 1){
        printf("Source: %d:%d | Dest: %d:%d | Seq: %d already exists...\n",
        vector_rec->message[i].hdr.message_id.src.u8[0], vector_rec->message[i].hdr.message_id.src.u8[1],
        vector_rec->message[i].hdr.message_id.dest.u8[0], vector_rec->message[i].hdr.message_id.dest.u8[1],
        vector_rec->message[i].hdr.message_id.seq);
      }
      else if (flag == 0){
        printf("Soure: %d:%d | Dest: %d:%d | Seq: %d is being added to the message_list...\n",
        vector_rec->message[i].hdr.message_id.src.u8[0], vector_rec->message[i].hdr.message_id.src.u8[1],
        vector_rec->message[i].hdr.message_id.dest.u8[0], vector_rec->message[i].hdr.message_id.dest.u8[1],
        vector_rec->message[i].hdr.message_id.seq);
        if(list_length(messages_list) < 5){
          new = memb_alloc(&messages_memb);
          memcpy(&new->message, &vector_rec->message[i], sizeof(dtn_message));
          printf("%d\n", new->message.hdr.message_id.src.u8[1]);
          list_add(messages_list, new);
        }
        else if (list_length(messages_list) >= 5){
          printf("5 reached.. removing\n");
          //r = list_head(messages_list);
          list_chop(messages_list);
          //memb_free(&messages_memb, r);
          new = memb_alloc(&messages_memb);
          memcpy(&new->message, &vector_rec->message[i], sizeof(dtn_message));
          list_add(messages_list, new);
        }
      }
    }
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
/* Sending out a broadcast */
PROCESS_THREAD(broadcast_process, ev, data)
{
    static struct etimer et;
    static uint8_t seqno;
    dtn_summary_vector msg;
    dtn_header header;
    dtn_msg_id inject;
    rimeaddr_t node_addr;
    struct neighbor *n;
    int randneighbor;
    int i;
    /* Specify the header values */
    header.ver = 1;
    header.type = 1;
    header.len = 1;

    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
    /* Lets go */
    PROCESS_BEGIN();
    node_addr.u8[0] = 128;
    node_addr.u8[1] = 11;
    rimeaddr_set_node_addr(&node_addr);

    printf("My address is: %d.%d\n",node_addr.u8[0], node_addr.u8[1]);
    /* Open the connection */
    broadcast_open(&broadcast, 129, &broadcast_call);
    set_power(0x01);

    /* Always going to be true */
    while(1) {
      /* Send a broadcast every 16 - 32 seconds */
      etimer_set(&et, CLOCK_SECOND * 3 + random_rand() % (CLOCK_SECOND * 3));
      /* Hold until the timer runs out */
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      if(list_length(neighbors_list) > 0) {
        randneighbor = random_rand() % list_length(neighbors_list);
        n = list_head(neighbors_list);
        for(i = 0; i < randneighbor; i++) {
            n = list_item_next(n);
        }
      }
      inject.dest = n->addr;
      inject.src =  rimeaddr_node_addr;
      inject.seq = 111;

      // printf("%d.%d|%d.%d|%d\n",
      // inject.src.u8[0], inject.src.u8[1],
      // inject.dest.u8[0], inject.dest.u8[1],
      // inject.seq);
      // n->summary.message_ids[0].dest.u8[0],
      // n->summary.message_ids[0].dest.u8[1]);
      msg.header = header;
      msg.message_ids[0] = inject;
      //place some values here
      packetbuf_copyfrom(&msg, sizeof(dtn_summary_vector));
      broadcast_send(&broadcast);
    }
    /* Done */
    PROCESS_END();
}


/*---------------------------------------------------------------------------*/
/* Send out a reliable unicast */
PROCESS_THREAD(runicast_process, ev, data)
{
    PROCESS_EXITHANDLER(runicast_close(&runicast);)
    /* Lets go */
    PROCESS_BEGIN();
    /* Open the connection */
    runicast_open(&runicast, 144, &runicast_callbacks);
    /* Always true */
    while(1) {
      /* Create instance of eTimer */
      static struct etimer et;
      /* Create instance of Unicast */
      struct runicast_message msg;
      /* Create instance of Neighbour */
      struct neighbor *n;
      int randneighbor, i;
      /* Set the random timer */
      etimer_set(&et, CLOCK_SECOND * 8 + random_rand() % (CLOCK_SECOND * 8));
      /* Wait for the timer to expire */
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      /* Pick a random neighbor from our list and send a unicast message to it. */
      if(list_length(neighbors_list) > 0) {
        randneighbor = random_rand() % list_length(neighbors_list);
        n = list_head(neighbors_list);
        for(i = 0; i < randneighbor; i++) {
            n = list_item_next(n);
        }
        /* Print the action */
        printf("Sending runicast to %d.%d\n", n->addr.u8[0], n->addr.u8[1]);
        msg.type = 1;
        msg.from = 2;
        packetbuf_copyfrom(&msg, sizeof(msg));
        /* unicast_send(&unicast, &n->addr); */
        runicast_send(&runicast, &n->addr, MAX_RETRANSMISSIONS);
      }
    }
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
