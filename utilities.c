#include <stdio.h>

convert_time(clock_time_t time){
  return time / 100;
};

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
