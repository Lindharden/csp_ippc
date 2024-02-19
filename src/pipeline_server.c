#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <dirent.h>
#include <ftw.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <csp/arch/csp_time.h>

#include <csp_pipeline_config/pipeline_server.h>

void pipeline_server_loop(void * param) {

	/* Statically allocate a listener socket */
	static csp_socket_t pipeline_server_socket = {0};

	/* Bind all ports to socket */
	csp_bind(&pipeline_server_socket, PIPELINE_PORT_SERVER);

	/* Create 10 connections backlog queue */
	csp_listen(&pipeline_server_socket, 10);

	/* Pointer to current connection and packet */
	csp_conn_t *conn;

	/* Process incoming connections */
	while (1) {

		/* Wait for connection, 10000 ms timeout */
		if ((conn = csp_accept(&pipeline_server_socket, CSP_MAX_DELAY)) == NULL) {
			continue;
        }

        printf("Server: Recieved connection on %d\n", csp_conn_dport(conn));

		/* Handle RDP service differently */
		if (csp_conn_dport(conn) == PIPELINE_PORT_SERVER) {
            int should_continue = true;
            do {
                should_continue = pipeline_server_handler(conn);
            } while (should_continue == true);
			
		} else {
            printf("Server: Unrecognized connection port %d\n", csp_conn_dport(conn));
        }

		/* Close current connection, and handle next */
        printf("Server: Closed connection on %d\n", csp_conn_dport(conn));
		csp_close(conn);
	}
}

int pipeline_server_handler(csp_conn_t * conn)
{
	// // Read request
	// csp_packet_t * packet = csp_read(conn, PIPELINE_SERVER_TIMEOUT);
	// if (packet == NULL) {
    //     printf("Server: Recieved no further requests\n");
	// 	return false;
    // }

	// // Copy data from request
	// pipeline_request_t * request = (void *) packet->data;

    // // The request is not of an accepted type
    // if (request->version > PIPELINE_VERSION) {
    //     printf("Server: Unknown requestion version %d. Expected %d or less\n", request->version, pipeline_VERSION);
    //     csp_buffer_free(packet);
    //     return false;
    // }

	// int type = request->type;

	// if (type == PIPELINE_SERVER_DOWNLOAD) {
    //     printf("Server: Handling download request\n");
    //     handle_server_download(conn, request);
	// } else if (request->type == PIPELINE_SERVER_UPLOAD) {
    //     printf("Server: Handling upload request\n");
	// 	handle_server_upload(conn, request);
    // } else if (request->type == PIPELINE_SERVER_LIST) {
    //     printf("Server: Handling list request\n");
	// 	handle_server_list(conn, request);
    // } else if (request->type == PIPELINE_MOVE) {
    //     printf("Server: Handling move request\n");
	// 	handle_server_move(conn, request);
    // } else if (request->type == PIPELINE_REMOVE) {
    //     printf("Server: Handling remove request\n");
	// 	handle_server_remove(conn, request);
    // } else if (request->type == PIPELINE_PERFORMANCE_UPLOAD) {
    //     printf("Server: Handling performance upload request\n");
	// 	perf_upload(conn, &request->perf_header);
    // } else if (request->type == PIPELINE_PERFORMANCE_DOWNLOAD) {
    //     printf("Server: Handling performance download request\n");
	// 	perf_download(conn, &request->perf_header);
    // } else {
    //     printf("Server: Unknown request\n");
    //     // If it fails to recognize request then the connection should be closed
    //     return false;
	// }

    // csp_buffer_free(packet);

    return true;
}