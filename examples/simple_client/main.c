#include <stdio.h>
#include "net/af.h"
#include "net/protnum.h"
#include "net/ipv6/addr.h"
#include "net/sock/udp.h"
#include "xtimer.h"

uint8_t buf[32];
int main(void)
{
    sock_udp_ep_t local = SOCK_IPV6_EP_ANY;
    sock_udp_t sock;
    local.port = 0xabcd;
    if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
        puts("Error creating UDP sock");
        return 1;
    }

    int i=0;
    uint8_t msg[8] = {'H','E','L','L','O',' ','0',0};
    uint8_t li = 0;
    while (1) {
        i++;

        sock_udp_ep_t remote = { .family = AF_INET6 };
        ssize_t res;
        remote.port = 12345;
        ipv6_addr_set_all_nodes_multicast((ipv6_addr_t *)&remote.addr.ipv6,
                                          IPV6_ADDR_MCAST_SCP_LINK_LOCAL);
        li = (li +1) %26;
        msg[6] = 'A'+li;
        if (sock_udp_send(&sock, msg, sizeof("Hello!"), &remote) < 0) {
            puts("Error sending message");
            sock_udp_close(&sock);
            return 1;
        }

        if ((res = sock_udp_recv(&sock, buf, sizeof(buf), 1 * US_PER_SEC,
                                 NULL)) < 0) {
            if (res == -ETIMEDOUT) {
                puts("Timed out");
            }
            else {
                puts("Error receiving message");
            }
        }
        else {
            printf("Received message: \"");
            for (int i = 0; i < res; i++) {
                printf("%c", buf[i]);
            }
            printf("\"\n");
        }

        xtimer_usleep(10); // 0.1 sec
    }
    return 0;
}