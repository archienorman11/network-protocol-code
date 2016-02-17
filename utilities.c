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

convert_time(clock_time_t time){
  return time / 100;
}

print_messages_list(){
    if(list_length(messages_list) > 0) {
        t = list_head(messages_list);
        printf("Length: %d\n", list_length(messages_list) );
        current_time = clock_time();
        for(i = 0; i < list_length(messages_list); i++) {
            //for(tmp = list_head(messages_list); tmp != NULL; tmp = list_item_next(tmp))
            printf("message_list_item --- Source: %d.%d | Dest: %d.%d | Seq: %d M:%s\n",
                   t->message.hdr.message_id.src.u8[0], t->message.hdr.message_id.src.u8[1],
                   t->message.hdr.message_id.dest.u8[0], t->message.hdr.message_id.dest.u8[1],
                   t->message.hdr.message_id.seq, t->message.msg);
            t = list_item_next(t);
        }
    }
    else {
        printf("No messages in the list\n");
    }
}

print_neighbour_list(){
    if(list_length(neighbors_list) > 0) {
        n = list_head(neighbors_list);
        current_time = clock_time();
        for(i = 0; i < list_length(neighbors_list); i++) {
            printf("neighbour_list_item: %d.%d, it was received at: %d seconds \n", /*--- Source: %d.%d | Dest: %d.%d | Seq: %d*/
                   n->addr.u8[0], n->addr.u8[1],
                   convert_time(n->timestamp)
                    /*n->summary.message_ids[i].src.u8[0],
                    n->summary.message_ids[i].src.u8[1],
                    n->summary.message_ids[i].dest.u8[0],
                    n->summary.message_ids[i].dest.u8[1],
                    n->summary.message_ids[i].seq*/);
            n = list_item_next(n);
        }
    }
    else {
        printf("No neighbours in the list\n");
    }
}