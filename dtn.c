/**
* @file dtn.c
* @author Archie Norman
* @date 26th Feb 2016
* @brief Contiki is a open source,efficient operating system, designed for sensor networks
* withlimited computing resources. This code implements the Spray and Wait algorithm,
* de-scribes a protocol design for the Contiki OS and its implementation focusing mainly on
* the best-effort and reliable communication abstractions.
*/
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
///Define the global structures
static struct broadcast_conn broadcast;
static struct runicast_conn runicast;
static struct etimer et;
static clock_time_t current_time;
///Glboal variables to store transmission metrics
int acks;
int timeouts;
int total_unicast_sent;
///The was used to extract message summary information.
static void
print_msg_id(dtn_msg_id *id)
{
 printf("<%d.%d:%d.%d:%d>",
   id->src.u8[0], id->src.u8[1],
   id->dest.u8[0], id->dest.u8[1],
   id->seq);
}
///This MEMB() definition defines a memory pool from which we allocate message entries.
MEMB(messages_memb, dtn_vector_list, MAX_MESSAGES);
///The neighbors_list is a Contiki list that holds the messages we have seen thus far.
LIST(messages_list);
///Delcare the prorcess used.
PROCESS(broadcast_process, "Broadcast process");
PROCESS(button_actions, "Buttons process");
///The AUTOSTART_PROCESSES() definition specifices what processes to start when this module is loaded. We put both our processes there.
AUTOSTART_PROCESSES(&broadcast_process, &button_actions);
/*
 *@param1 - Broadcast receive function takes a pointer to the delared broadcast connetion struct
 *@param2 - from address as parareters.
 */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  /*
   *Create a pointer of type dtn_summary_vector,
   *this is what the received boradcast will be stored in.
   *tmp will be used to iterate through the messages cache
   *dtn_vector will be used to send a unicast if there are messages
   *missing in the (received) nodes message cache
   */
  dtn_summary_vector *broadcast_received;
  dtn_vector_list *tmp;
  static dtn_vector unicast_message;
  ///Returns a pointer to the data in the packet buffer and assign it to broadcast receive
  broadcast_received = packetbuf_dataptr();
  int flag, i, b, a, d;
  b = 0;
  ///Sanity check to mkae sure the data we receive is correct
  printf("--- [R-BC] From: %d.%d *** \n",
    from->u8[0], from->u8[1]
    // broadcast_received->message_ids[i].src.u8[0], broadcast_received->message_ids[i].src.u8[1],
    // broadcast_received->message_ids[i].dest.u8[0], broadcast_received->message_ids[i].dest.u8[1],
    // broadcast_received->message_ids[i].seq
    );
  /*
   *Assign the first element in the messages cache to
   *tmp and iterate through each element.
   *At each iteration we must then iterate through each
   *of the summary vector elements
   */
  for(tmp = list_head(messages_list); tmp != NULL; tmp = list_item_next(tmp)) {
    for (i = 0; i < broadcast_received->header.len; i++) {
      ///Check to see which messages the neighbour already has
      if ((rimeaddr_cmp(&broadcast_received->message_ids[i].src, &tmp->message.hdr.message_id.src) &&
        rimeaddr_cmp(&broadcast_received->message_ids[i].dest, &tmp->message.hdr.message_id.dest) &&
        broadcast_received->message_ids[i].seq == tmp->message.hdr.message_id.seq)) {
        // printf("--- [ALERT] %d.%d already has: Src: %d.%d | Dest: %d.%d | Seq: %d | Msg: %s --- \n",
        //   from->u8[0], from->u8[1],
        //   tmp->message.hdr.message_id.src.u8[0], tmp->message.hdr.message_id.src.u8[1],
        //   tmp->message.hdr.message_id.dest.u8[0], tmp->message.hdr.message_id.dest.u8[1],
        //   tmp->message.hdr.message_id.seq,
        //   tmp->message.msg
        //   );
        break;
      }
    }
    if(i == broadcast_received->header.len) {
      ///Check to see if the there is only one copy left of this message
      if(tmp->message.hdr.number_of_copies == 1) {
        /// Only one left, check to see if they are the destination address
        if(!rimeaddr_cmp(&tmp->message.hdr.message_id.dest, from)) {
          printf("--- [ALERT] This neighbour does not have this message, but they are not the destination and there is only one copy left\n");
          continue;
        }
      }
      ///Add the message in my cache to the unicast message so its ready for sending
      unicast_message.message[b] = tmp->message;
      ///Make sure we are not sending a 0 value for the number of copies remaining.
      if (unicast_message.message[b].hdr.number_of_copies != 1) {
        ///Halve the number of copies before sending
        unicast_message.message[b].hdr.number_of_copies /= 2;
      }
      b++;
    }
  }
  if(b == 0) {
    return;
  }
  ///Set the message type and the len of the packet
  unicast_message.header.type = DTN_MESSAGE;
  unicast_message.header.len = b;
  ///Make sure that the buffer is not already being used by runicast
  if(!runicast_is_transmitting(&runicast)) {
    ///Sanity check, print each message in the uniacst packet
    for (d = 0; d < unicast_message.header.len; d++) {
    printf("--- [S-UC] Src: %d.%d | Dest %d.%d | Seq: %d | Copies %d | Timestamp %d | Len: %d  ---- \n",
      unicast_message.message[d].hdr.message_id.src.u8[0], unicast_message.message[d].hdr.message_id.src.u8[1],
      unicast_message.message[d].hdr.message_id.dest.u8[0], unicast_message.message[d].hdr.message_id.dest.u8[1],
      unicast_message.message[d].hdr.message_id.seq,
      unicast_message.message[d].hdr.number_of_copies,
      unicast_message.message[d].hdr.timestamp,
      unicast_message.header.len
      );
    }
    ///Copy the runicast message in to the packet buffer
    packetbuf_copyfrom(&unicast_message, sizeof(dtn_vector));
    ///Assign a maximum retransmuission and send
    runicast_send(&runicast, from, MAX_RETRANSMISSIONS);
    ///Increment for testing purposes
    total_unicast_sent ++;
  }
}
/*
 *This is where we define what function to be called when a broadcast is received.
 *We pass a pointer to this structure in the broadcast_open() call below.
 */
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
///This function is called for every incoming unicast packet.
static void recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
  ///Store the unicast we receive
  dtn_vector *unicast_recieved;
  dtn_vector_list *add_to_list, *tmp_head, *final_destination_check;
  int i;
  ///Returns a pointer to the data in the packet buffer and assign it to unicast received
  unicast_recieved = packetbuf_dataptr();
  ///Iterate through the messages in the packet
  for (i = 0; i < unicast_recieved->header.len; i++) {
    printf("--- [R-UC] Src: %d.%d | Dest: %d.%d | Copies: %d | Timestamp: %d | Msg %s ---\n",
      unicast_recieved->message[i].hdr.message_id.src.u8[0], unicast_recieved->message[i].hdr.message_id.src.u8[1],
      unicast_recieved->message[i].hdr.message_id.dest.u8[0], unicast_recieved->message[i].hdr.message_id.dest.u8[1],
      unicast_recieved->message[i].hdr.number_of_copies, unicast_recieved->message[i].hdr.timestamp,
      unicast_recieved->message[i].msg);
      ///If the node has my address, consume the message
      if (unicast_recieved->message[i].hdr.message_id.dest.u8[1] == 9) {
        printf(" ********** Final desination reached **********\t --- Src: %d.%d | Dest: %d.%d | Copies: %d | Timestamp: %d | Msg *%s* ---\n",
        unicast_recieved->message[i].hdr.message_id.src.u8[0], unicast_recieved->message[i].hdr.message_id.src.u8[1],
        unicast_recieved->message[i].hdr.message_id.dest.u8[0], unicast_recieved->message[i].hdr.message_id.dest.u8[1],
        unicast_recieved->message[i].hdr.number_of_copies, unicast_recieved->message[i].hdr.timestamp,
        unicast_recieved->message[i].msg);
        ///Pre-agreed format for testing purposes
        printf("[RCV-RCH] ");
        print_msg_id(&unicast_recieved->message[i].hdr.message_id);
        printf(" from %d.%d", from->u8[0], from->u8[1]);
        printf(" --%d\n", clock_seconds());
      }
      else {
        ///unicast_recieved->message[i].hdr.number_of_copies = (unicast_recieved->message[i].hdr.number_of_copies / 2);
        ///Check to see if there is space in the message cache, if so add it to the front.
        if(list_length(messages_list) < 5){
          add_to_list = memb_alloc(&messages_memb);
          memcpy(&add_to_list->message, &unicast_recieved->message[i], sizeof(dtn_message));
          list_add(messages_list, add_to_list);
        }
        /*
         *If there is not space, pop the last element, deallocate the memory,
         *assign new memory and add the new message to the front
         */
        else if (list_length(messages_list) >= 5){
          printf("--- [ALERT] Popping last element\n");
          tmp_head = list_pop(messages_list);
          memb_free(&messages_memb, tmp_head);
          add_to_list = memb_alloc(&messages_memb);
          memcpy(&add_to_list->message, &unicast_recieved->message[i], sizeof(dtn_message));
          list_add(messages_list, add_to_list);
        }
      }
    }
}

/*
 * @brief This is the callback function, this tells us when a message has been delivered
 * @param1 - the runiast connection parameter
 * @param2 - the destination node of the original runicast messagea
 * @param3 - the number of transmissions
 */
static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  dtn_vector_list *final_destination_check;
  acks ++;
  printf("--- [ALERT] ******** SUCCESSFULLLY SENT TO %d.%d | TMS; %d ********\n", to->u8[0], to->u8[1], retransmissions);
  ///Iterate through the messagea cache
  final_destination_check = list_head(messages_list);
  for(final_destination_check = list_head(messages_list); final_destination_check != NULL; final_destination_check = list_item_next(final_destination_check)) {
    ///If the message was sent to its final destination then we should remove it from the list
    if (rimeaddr_cmp(&final_destination_check->message.hdr.message_id.dest, to)) {
      printf("--- [ALERT] Sent to final destination, cleaning the message list.\n");
      list_remove(messages_list, final_destination_check);
      memb_free(&messages_memb, final_destination_check);
    }
    ///Halve the number of copies in the message list upon acknowledgement
    else {
      final_destination_check->message.hdr.number_of_copies /= 2;
    }
  }
}
/*
 * @brief Keeps track of the number of timeouts
 * @param1 - the runiast connection parameter
 * @param2 - the destination node of the original runicast messagea
 * @param3 - the number of transmissions
 */
static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  printf("--- [ALERT] Runicast message timed out when sending to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
  ///Used for testing purposes
  timeouts ++;
}
static const struct runicast_callbacks runicast_callbacks = {recv_runicast, sent_runicast, timedout_runicast};
static struct runicast_conn runicast;
/*
 * @brief Single protohead, called when an event occurs
 * @param1 - the defined process parameter
 * @param2 - the event
 * @param3 - the number of transmissions
 */
PROCESS_THREAD(broadcast_process, ev, data)
{
  ///Define strutures and variables used in the process
  static struct etimer et;
  int i, h;
  static dtn_summary_vector send;
  dtn_vector_list *my_vector;
  dtn_header header;
  rimeaddr_t node_addr;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();
  ///Define the power used for testing and the node address to use
  set_power(1);
  node_addr.u8[0] = 128;
  node_addr.u8[1] = 9;
  rimeaddr_set_node_addr(&node_addr);
  ///First open the broadcast and unicast connections and assign the channels used
  broadcast_open(&broadcast, 229, &broadcast_call);
  runicast_open(&runicast, 244, &runicast_callbacks);
  ///Keep looping
  while(1) {
      ///Define the randon time period with broadcast within
      etimer_set(&et, CLOCK_SECOND * 2 + random_rand() % (CLOCK_SECOND * 5));
      ///Block until x seconds is reached
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      i = 0;
      ///Iterate through my messages cache
      for(my_vector = list_head(messages_list); my_vector != NULL; my_vector = list_item_next(my_vector)) {
        // printf("--- [BC VECTOR ITEM]: Src: %d.%d | Dest: %d.%d | Seq: %d --- \n",
        // my_vector->message.hdr.message_id.src.u8[0], my_vector->message.hdr.message_id.src.u8[1],
        // my_vector->message.hdr.message_id.dest.u8[0], my_vector->message.hdr.message_id.dest.u8[1],
        // my_vector->message.hdr.message_id.seq);

        /*Add each message in the cache to a summary vector to be
         *sent out in the broadcast
         * and increment the array index value
         */
        send.message_ids[i++] = (my_vector->message.hdr.message_id);
      }
      ///Assign the type and the number of messages in the vector
      header.type = DTN_SUMMARY_VECTOR;
      header.len = i;
      send.header = header;
      send.message_ids;
      ///Make sure runicast is not already transmitting
      if(!runicast_is_transmitting(&runicast)) {
        ///Make sure the size is same expected structure
        packetbuf_copyfrom(&send, sizeof(dtn_summary_vector));
        ///Send the broadcast
        broadcast_send(&broadcast);
      }
  }
  PROCESS_END();
}
/*This process is used to test runicast recieve, inject messages in to cache
 *and to print out the message cache at a given time
 */
PROCESS_THREAD(button_actions, ev, data)
{
  ///Declarations
  rimeaddr_t node_addr, dest_addr;
  static dtn_summary_vector sim_broadcast;
  static dtn_vector sim_unicast;
  static dtn_vector_list *m;
  dtn_header header;
  int i, b;
  PROCESS_BEGIN();
  ///Activate the buttons for use
  SENSORS_ACTIVATE(button_sensor);
  SENSORS_ACTIVATE(button2_sensor);
  while(1) {
    ///Wait for a button to be clicked
    PROCESS_WAIT_EVENT_UNTIL((ev == sensors_event && data == &button_sensor) ||
                            (ev == sensors_event && data == &button2_sensor));
      ///Check which button has been clicked
      if (ev == sensors_event && data == &button_sensor) {
        ///Make sure runiacst isnt broadcasting before we call the receive function
        if(!runicast_is_transmitting(&runicast)) {
          ///Pack the message
          rimeaddr_copy(&node_addr, &rimeaddr_null);
          node_addr.u8[0] = 128;
          node_addr.u8[1] = 9;
          header.ver = 3;
          header.type = 2;
          header.len = 1;
          rimeaddr_copy(&dest_addr, &rimeaddr_null);
          ///Pick a random destination, make sure I am not the destination
          do {
              dest_addr.u8[0] = 128;
              dest_addr.u8[1] = 1 + random_rand()%10;
              }
              while(dest_addr.u8[1] == 9);
          ///Add the (one) message to the simulate unicast variable
          for (i = 0; i < header.len; i++) {
            sim_unicast.message[i].hdr.message_id.dest = dest_addr;
            sim_unicast.message[i].hdr.message_id.src =  node_addr;
            sim_unicast.message[i].hdr.message_id.seq = i;
            sim_unicast.message[i].hdr.number_of_copies =  1;
            sim_unicast.message[i].hdr.timestamp =  clock_seconds();
            sim_unicast.message[i].hdr.length =  header.len;
            strncpy(sim_unicast.message[i].msg, "arch", 5);
            ///Print in the log aggregated format
            printf("[MSG-CRT] ");
            print_msg_id(&sim_unicast.message[i].hdr.message_id);
            printf(" --%d\n", clock_seconds());
          }
          sim_unicast.header = header;
          packetbuf_copyfrom(&sim_unicast, sizeof(dtn_vector));
          ///Call the receive unicast function and pass the packet
          recv_runicast(&runicast, &dest_addr, MAX_RETRANSMISSIONS);
      }
    }
    ///Print the message cache
    else if (ev == sensors_event && data == &button2_sensor){
      ///Check its not empty
      if(list_length(messages_list) > 0) {
        m = list_head(messages_list);
        ///Display some stats
        printf("TOT_UCST: %d | ACKS: %d | TMOUTS: %d | PERCENTAGE SUCCESS: %d \n" ,
        total_unicast_sent, acks, timeouts,  total_unicast_sent / acks * 100);
        ///Iterate through the cache
        for(m = list_head(messages_list); m != NULL; m = list_item_next(m)) {
          printf("--- [ALERT]: Src: %d.%d | Dest: %d.%d | Seq: %d | Msg: %s | Number of copies: %d --- \n",
          m->message.hdr.message_id.src.u8[0], m->message.hdr.message_id.src.u8[1],
          m->message.hdr.message_id.dest.u8[0], m->message.hdr.message_id.dest.u8[1],
          m->message.hdr.message_id.seq,
          m->message.msg,
          m->message.hdr.number_of_copies);
        }
      }
      else {
        printf("--- [ALERT][M LIST]: Empty\n");
      }
    }
  }
  PROCESS_END();
}
