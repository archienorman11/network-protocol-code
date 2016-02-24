#include <stdio.h>

convert_time(clock_time_t time){
  return time / 100;
};


/** PRINT NEIGHBORS **/

// else if (ev == sensors_event && data == &button2_sensor){
//   if(list_length(neighbors_list) > 0) {
//     n = list_head(neighbors_list);
//     current_time = clock_time();
//     for(i = 0; i < list_length(neighbors_list); i++) {
//       printf("neighbour_list_item: %d.%d, it was received at: %d seconds \n", /*--- Source: %d.%d | Dest: %d.%d | Seq: %d*/
//       n->addr.u8[0], n->addr.u8[1],
//       convert_time(n->timestamp)
//       /*n->summary.message_ids[i].src.u8[0],
//       n->summary.message_ids[i].src.u8[1],
//       n->summary.message_ids[i].dest.u8[0],
//       n->summary.message_ids[i].dest.u8[1],
//       n->summary.message_ids[i].seq*/);
//       n = list_item_next(n);
//     }
//   }
//   else {
//     printf("No neighbours in the list\n");
//   }
// }


/** PRINT MESSAGES LIST **/

//if(list_length(messages_list) > 0) {
//t = list_head(messages_list);
//printf("Length: %d\n", list_length(messages_list) );
//current_time = clock_time();
//  for(i = 0; i < list_length(messages_list); i++) {
//    //for(tmp = list_head(messages_list); tmp != NULL; tmp = list_item_next(tmp))
//    printf("message_list_item --- Source: %d.%d | Dest: %d.%d | Seq: %d M:%s\n",
//    t->message.hdr.message_id.src.u8[0], t->message.hdr.message_id.src.u8[1],
//    t->message.hdr.message_id.dest.u8[0], t->message.hdr.message_id.dest.u8[1],
//    t->message.hdr.message_id.seq, t->message.msg);
//    t = list_item_next(t);
//  }
//}
//else {
//  printf("No messages in the list\n");
//}

/** Simulate broadcast Receive **/ 

// for (i = 0; i < header.len; i++) {
//   rimeaddr_copy(&dest_addr, &rimeaddr_null);
//   dest_addr.u8[0] = 128;
//   dest_addr.u8[1] = 1 + (random_rand() % 5);
//   sim_broadcast.message_ids[i].dest = dest_addr;
//   sim_broadcast.message_ids[i].src =  node_addr;
//   sim_broadcast.message_ids[i].seq = 1;
// }
// sim_broadcast.header = header;
// packetbuf_copyfrom(&sim_broadcast, sizeof(dtn_summary_vector));
//broadcast_recv(&broadcast, &node_addr);
/* Create an example variable capable of holding 50 characters */
