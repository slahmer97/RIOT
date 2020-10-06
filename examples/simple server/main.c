

#include <stdio.h>

#include <stdio.h>
#include <inttypes.h>

#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netif/hdr.h"
#include "net/gnrc/udp.h"
#include "net/gnrc/pktdump.h"
#include "timex.h"
#include "utlist.h"
#include "xtimer.h"
#include "net/sock/udp.h"

/*
 * @TYPE = 1 --> server keeps reading new messages from the socket (if exists)
 * @TYPE = 2 --> server keeps reading new messages every 2 seconds (if exists)
 * @TYPE = 3 --> server has N thread workers, for each new message, it creates a new thread if available among the N ones.
 *                  if not, he waits until a worker is available (serving multiple request in the same time)
 */
#define TYPE 3
#define MAX_BUFF 256
sock_udp_t sock;


static int die(const char * err){
    perror(err);
    exit(1);
}
#if TYPE == 3
#include "thread.h"
#include <mutex.h>
#define WORKERS_COUNT 2

/*
 * passed to each thread on creation
 */
struct meta_data_t{
    /*
     * passed to each thread so he can know which rcv_thread_stack page to be freed when it finishes
     */
    int cell;
} meta_data_var[WORKERS_COUNT];


struct {
    uint8_t count;
    mutex_t lock;

    /*
     * a bitmap for used rcv_thread_stack can be used instead
     * here, only the case of two threads works
     */
    uint8_t first_cell : 1;
    uint8_t second_cell : 1;
}my_struct_var;
char rcv_thread_stack[WORKERS_COUNT][THREAD_STACKSIZE_MAIN];

void *rcv_thread(void *arg)
{
    uint8_t buff[MAX_BUFF];
    (void) arg;
    struct meta_data_t* hum = ( struct meta_data_t*) arg;
    printf("Thread worker %d started\n",hum->cell);

    sock_udp_ep_t remote;
    ssize_t res;
    if ((res = sock_udp_recv(&sock, buff, sizeof(buff), SOCK_NO_TIMEOUT,
                             &remote)) >= 0) {
        if(res) {
            printf("Received : %.*s\n", (int) (res <= MAX_BUFF ? res : MAX_BUFF), buff);
            if (sock_udp_send(&sock, "PIW ^^ TYPE 3", 14, &remote) < 0) {
                puts("Error sending reply");
            }

        }
    }
    else{
        puts("res <0");
    }
    xtimer_sleep(2); // do something
    mutex_lock(&my_struct_var.lock);
    //set the cell unused
    if(hum->cell == 0)
        my_struct_var.first_cell = 0;
    else
        my_struct_var.second_cell = 0;
    my_struct_var.count--;
    printf("Thread worker %d exited\n",hum->cell);
    mutex_unlock(&my_struct_var.lock);

    return NULL;
}
#endif




int main(void)
{
#if TYPE == 3
    my_struct_var.count = 0;
    my_struct_var.second_cell = 0;
    my_struct_var.first_cell = 0;
    mutex_init(&my_struct_var.lock);
#endif

    uint8_t buff[MAX_BUFF];
    buff[0] = 0;
    if(buff[0] == 0)
        buff[0] =0;
    sock_udp_ep_t local = SOCK_IPV6_EP_ANY;

    local.port = 12345;
    if (sock_udp_create(&sock, &local, NULL, 0) < 0)
        return die("[-] Error udp_sock_create\n");

    while (1) {
       printf("[+] Loop--------------\n");

#if TYPE == 1 || TYPE == 2

        sock_udp_ep_t remote;
        ssize_t res;
        if ((res = sock_udp_recv(&sock, buff, sizeof(buff), SOCK_NO_TIMEOUT,
                                 &remote)) >= 0) {
            puts("Received a message");
            if(res) {
                printf("%.*s\n", (int) (res <= MAX_BUFF ? res : MAX_BUFF), buff);
                if (sock_udp_send(&sock, "PIW ^^ TYPE 1 OR 2", 19, &remote) < 0) {
                    puts("Error sending reply");
                }
            }
        }
        else{
            puts("res <0");
        }
    #if TYPE == 2
            puts("going to sleep for 1sec");
            xtimer_sleep(1);
    #endif

#elif TYPE == 3
        mutex_lock(&my_struct_var.lock);
        if(my_struct_var.count == WORKERS_COUNT) {
            // do nothing
            // no thread is available
            printf("[-] No thread worker is available\n");
            //can use a sem instead of this
            xtimer_sleep(1);
        }
        else {
            int cell = (my_struct_var.first_cell == 0) ? 0 : 1;
            meta_data_var[cell].cell = cell;
            if(cell == 0)
                my_struct_var.first_cell = 1;
            else
                my_struct_var.second_cell = 1;

            my_struct_var.count++;
            /*rcv_pid = */thread_create(&rcv_thread_stack[cell][0], THREAD_STACKSIZE_MAIN,
                                        THREAD_PRIORITY_MAIN - 1, 0, rcv_thread, (void*)&meta_data_var[cell], "rcv");

        }
        mutex_unlock(&my_struct_var.lock);
#else
        puts("type doesn't exists");
        exit(0);
#endif

    }
    return 0;
}
