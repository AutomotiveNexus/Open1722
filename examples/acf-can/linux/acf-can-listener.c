/*
 * Copyright (c) 2024, COVESA
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of COVESA nor the names of its contributors may be
 *      used to endorse or promote products derived from this software without
 *      specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <argp.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <linux/can/raw.h>
#include <sys/ioctl.h>

#include "common/common.h"
#include "avtp/Udp.h"
#include "avtp/acf/Ntscf.h"
#include "avtp/acf/Tscf.h"
#include "avtp/acf/AcfCommon.h"
#include "avtp/acf/Can.h"
#include "avtp/CommonHeader.h"
#include "acf-can-common.h"

#define ARGPARSE_CAN_FD_OPTION          500
#define ARGPARSE_CAN_IF_OPTION          501
#define ARGPARSE_LISTENER_ID_OPTION     503
#define STREAM_ID                       0xAABBCCDDEEFF0001

static char ifname[IFNAMSIZ];
static uint8_t macaddr[ETH_ALEN];
static uint8_t use_udp;
static uint32_t udp_port = 17220;
static Avtp_CanVariant_t can_variant = AVTP_CAN_CLASSIC;
static char can_ifname[IFNAMSIZ];
static uint64_t listener_stream_id = STREAM_ID;

static char doc[] =
        "\nacf-can-listener -- a program to receive CAN messages from a remote CAN bus over Ethernet using Open1722.\
        \vEXAMPLES\n\
        acf-can-listener -i eth0 -d aa:bb:cc:dd:ee:ff --canif can1\n\
        \t(tunnel Open1722 CAN messages received from eth0 to can1)\n\
        acf-can-listener --canif can1 -u -p 17220\n\
        \t(tunnel Open1722 CAN messages received over UDP from port 17220 to can1)";

static struct argp_option options[] = {
    {"udp", 'u', 0, 0, "Use UDP (Default: Ethernet)" },
    {"fd", ARGPARSE_CAN_FD_OPTION, 0, 0, "Use CAN-FD"},
    {"canif", ARGPARSE_CAN_IF_OPTION, "CAN_IF", 0, "CAN interface"},
    {"ifname", 'i', "IFNAME", 0, "Network interface (If Ethernet)"},
    {"dst-addr", 'd', "MACADDR", 0, "Stream destination MAC address (If Ethernet)"},
    {"udp-port", 'p', "UDP_PORT", 0, "UDP Port to listen on (if UDP)"},
    {"stream-id", ARGPARSE_LISTENER_ID_OPTION, "STREAM_ID", 0, "Stream ID for listener stream"},
    { 0 }
};

static error_t parser(int key, char *arg, struct argp_state *state)
{
    int res;

    switch (key) {
    case 'p':
        udp_port = atoi(arg);
        break;
    case 'u':
        use_udp = 1;
        break;
    case ARGPARSE_CAN_FD_OPTION:
        can_variant = AVTP_CAN_FD;
        break;
    case ARGPARSE_CAN_IF_OPTION:
        strncpy(can_ifname, arg, sizeof(can_ifname) - 1);
        break;
    case 'i':
        strncpy(ifname, arg, sizeof(ifname) - 1);
        break;
    case 'd':
        res = sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &macaddr[0], &macaddr[1], &macaddr[2],
                &macaddr[3], &macaddr[4], &macaddr[5]);
        if (res != 6) {
            fprintf(stderr, "Invalid MAC address\n");
            exit(EXIT_FAILURE);
        }
        break;
    case ARGPARSE_LISTENER_ID_OPTION:
        res = sscanf(arg, "%lx", &listener_stream_id);
        if (res != 1) {
            fprintf(stderr, "Invalid talker stream id\n");
            exit(EXIT_FAILURE);
        }
        break;
    }

    return 0;
}

static struct argp argp = { options, parser, NULL, doc};

int main(int argc, char *argv[])
{
    int fd, res;
    int can_socket = 0;
    struct sockaddr_can can_addr;
    struct ifreq ifr;
    uint16_t pdu_length = 0, cf_length = 0;
    uint8_t num_can_msgs = 0;
    uint8_t exp_cf_seqnum = 0;
    uint32_t exp_udp_seqnum = 0;
    uint8_t pdu[MAX_ETH_PDU_SIZE];
    frame_t can_frames[MAX_CAN_FRAMES_IN_ACF];

    argp_parse(&argp, argc, argv, 0, NULL, NULL);
    // Print current configuration
    printf("acf-can-listener configuration:\n");
    if(can_variant == AVTP_CAN_CLASSIC)
        printf("\tUsing Classic CAN interface: %s\n", can_ifname);
    else if(can_variant == AVTP_CAN_FD)
        printf("\tUsing CAN FD interface: %s\n", can_ifname);
    if(use_udp) {
        printf("\tUsing UDP\n");
        printf("\tListening port: %d\n", udp_port);
    } else {
        printf("\tUsing Ethernet\n");
        printf("\tNetwork Interface: %s\n", ifname);
    }
    printf("\tListener Stream ID: 0x%lx\n", listener_stream_id);

    // Configure an appropriate socket: UDP or Ethernet Raw
    if (use_udp) {
        fd = create_listener_socket_udp(udp_port);
    } else {
        fd = create_listener_socket(ifname, macaddr, ETH_P_TSN);
    }

    if (fd < 0)
        return 1;

    // Open a CAN socket for reading frames
    can_socket = setup_can_socket(can_ifname, can_variant);
    if (can_socket < 0) goto err;

    // Start an infinite loop to keep converting AVTP frames to CAN frames
    for(;;) {

        pdu_length = recv(fd, pdu, MAX_ETH_PDU_SIZE, 0);
        if (pdu_length < 0 || pdu_length > MAX_ETH_PDU_SIZE) {
            perror("Failed to receive data");
            continue;
        }

        num_can_msgs = avtp_to_can(pdu, can_frames, can_variant, use_udp,
                             listener_stream_id, &exp_cf_seqnum, &exp_udp_seqnum);
        exp_cf_seqnum++;
        exp_udp_seqnum++;

        for (int i = 0; i < num_can_msgs; i++) {
            int res;
            if (can_variant == AVTP_CAN_FD)
                res = write(can_socket, &can_frames[i].fd, sizeof(struct canfd_frame));
            else if (can_variant == AVTP_CAN_CLASSIC)
                res = write(can_socket, &can_frames[i].cc, sizeof(struct can_frame));

            if(res < 0)
            {
                perror("Failed to write to CAN bus");
                continue;
            }
        }
    }

    return 0;

err:
    close(fd);
    return 1;

}
