#include "chatServer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/ioctl.h>


static int end_server = 0;

void intHandler(int SIG_INT) {
    end_server = 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: chatServer <port>\n");
        exit(EXIT_FAILURE);
    }
    int port = (int) strtol(argv[1], NULL, 10);
    if (port <= 0 || port > 65536) {
        printf("Usage: chatServer <port>\n");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, intHandler);//to catch if the user wants out.

    conn_pool_t *pool = (conn_pool_t *) malloc(sizeof(conn_pool_t));
    if (pool == NULL) {
        perror("pool FAIL");
        free(pool);
        exit(1);
    }
    init_pool(pool);
//open new socket like we need.
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket des FAIL");
        free(pool);
        close(fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr;
    int on = 1;
    int nonBlock = ioctl(fd, (int) FIONBIO, (char *) &on);//we want this socke non-block
    if (nonBlock < 0) {
        perror("ioctl FAIL");
        close(fd);
        free(pool);
        exit(EXIT_FAILURE);
    }//difine struct and host to netWork long&short

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    //bind&listen
    if (bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind FAIL");
        free(pool);
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (listen(fd, 5) < 0) {
        perror("listen FAIL");
        free(pool);
        close(fd);
        exit(EXIT_FAILURE);
    }
    FD_SET(fd, &pool->read_set);//turn on set bite
    /*************************************************************/
    /* Loop waiting for incoming connects, for incoming data or  */
    /* to write data, on any of the connected sockets.           */
    /*************************************************************/
    do {
        if (pool->maxfd == 0)
            pool->maxfd = fd;
        //copy the read set becouse we will use the copy, not the origin
        memcpy(&pool->ready_read_set, &pool->read_set, sizeof(pool->read_set));
        memcpy(&pool->ready_write_set, &pool->write_set, sizeof(pool->write_set));
        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
        //select on the current
        pool->nready = select((pool->maxfd) + 1, &pool->ready_read_set, &pool->ready_write_set, NULL, NULL);
        if (pool->nready < 0)
            end_server = 1;
        int count = 0;//adding the count param to have anoder condition.
        for (int i = 0; count < pool->nready && i <= pool->maxfd; i++) {
            if (FD_ISSET(i, &pool->ready_read_set)) {
                /***************************************************/
                /* Check to see if this descriptor is ready for read!!*/
                /* A descriptor was found that was readable		   */
                /* if this is the listening socket, accept one      */
                /* incoming connection that is queued up on the     */
                /*  listening socket before we loop back and call   */
                /* select again. 						            */
                /****************************************************/
                count++;
                if (i == fd) {
                    int accInt = accept(fd, NULL, NULL);
                    if (accInt < 0) {
                        close(fd);
                        //free(pool);
                    }
                    add_conn(accInt, pool);//collect the connection
                    printf("New incoming connection on sd %d\n", accInt);
                    (pool->nready)--;//we used the ready one so --;
                    continue;
                }
                printf("Descriptor %d is readable\n", i);
                char *buf = (char *) malloc((sizeof(char)) * (BUFFER_SIZE));
                if (buf == NULL) {//do not free buff here. ignore
                    close(fd);
                }
                memset(buf, '\0', BUFFER_SIZE);//fill with '\0' and read to it from fd
                long reader = read(i, buf, BUFFER_SIZE - 1);
                if (reader == 0) {
                    remove_conn(i, pool);
                    printf("Connection closed for sd %d\n", i);
                }
                printf("%ld bytes received from sd %d\n", reader, i);
                add_msg(i, buf, (int) reader, pool);//add msg to pool
                free(buf);
                (pool->nready)--;
            } /* End of if (FD_ISSET()) */
            /*******************************************************/
            /* Check to see if this descriptor is ready for write  */
            /*******************************************************/
            if (FD_ISSET(i, &pool->ready_write_set)) {
                /* try to write all msgs in queue to sd */
                write_to_client(i, pool);
                (pool->nready)--;
            }
            /*******************************************************/
        } /* End of loop through selectable descriptors */
    } while (end_server == 0);//while the user didn't press ctrl+c;

    /*************************************************************/
    /* If we are here, Control-C was typed,						 */
    /* clean up all open connections					         */
    /*************************************************************/
    printf("\n");
    while (pool->conn_head != NULL)
        remove_conn(pool->conn_head->fd, pool);
    printf("removing connection with sd %d \n", fd);
    close(fd);
    free(pool);//end of program
    return 0;
}


int init_pool(conn_pool_t *pool) {
    //init all pool param.
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->ready_write_set);
    pool->nr_conns = 0;
    pool->nready = 0;
    pool->maxfd = 0;
    pool->conn_head = NULL;//there is no head!
    return 0;
}

int add_conn(int sd, conn_pool_t *pool) {
    // alloc the new connection and init the parameters
    conn_t *conn = (conn_t *) malloc(sizeof(conn_t));
    conn->next = NULL;
    conn->prev = NULL;
    conn->write_msg_head = NULL;
    conn->write_msg_tail = NULL;
    conn->fd = sd;
    //update the maxfd
    if (pool->maxfd < sd)
        pool->maxfd = sd;

    if (pool->nr_conns == 0) { //if there is no new connections
        pool->conn_head = conn;
    }
    else {//if there was already connection place the new one at the top
        conn->next = pool->conn_head;
        pool->conn_head->prev = conn;
        pool->conn_head = conn;
    }
    //update the num of connections
    (pool->nr_conns)++;
    FD_SET(sd, &pool->read_set);
    return 0;
}

//this func will be used at the end of the program,
int remove_conn(int sd, conn_pool_t *pool) {
    printf("removing connection with sd %d \n", sd);
    conn_t *connPtr = pool->conn_head;
    for (; connPtr != NULL; connPtr = connPtr->next) {
        if (connPtr->fd == sd)
            break;
    }
    if (connPtr->next != NULL && connPtr->prev != NULL) {
        //if there is ptr behind and after the sd
        connPtr->next->prev = connPtr->prev;
        connPtr->prev->next = connPtr->next;
    } else if (connPtr->prev != NULL) {
        //if it's the last one
        connPtr->prev->next = NULL;
    } else if (connPtr->next != NULL) {
        //it's the first
        pool->conn_head = connPtr->next;
        pool->conn_head->prev = NULL;
    } else
        pool->conn_head = NULL;
    //after we fix all the pool issues lest start free the msg
    if (connPtr->write_msg_head != NULL) {
        msg_t *freeHead = connPtr->write_msg_head;
        msg_t *temp;
        for (; freeHead != NULL; freeHead = temp) {
            free(freeHead->message);
            temp = freeHead->next;
            free(freeHead);
        }
        FD_CLR(connPtr->fd, &pool->write_set);//clear write set
    }
    FD_CLR(connPtr->fd, &pool->read_set);//clear read set

    conn_t *connHead = pool->conn_head;//new ptr we can move on
    if (pool->maxfd == connPtr->fd) {
        pool->maxfd = 0;
        for (; connHead != NULL; connHead = connHead->next) {
            if (connHead->fd > pool->maxfd) {//update the maxfd size
                pool->maxfd = connHead->fd;
            }
        }
    }
    pool->nr_conns -= 1;
    close(connPtr->fd);
    free(connPtr);
    return 0;
}

int add_msg(int sd, char *buffer, int len, conn_pool_t *pool) {
    /*
	 * 1. add msg_t to write queue of all other connections
	 * 2. set each fd to check if ready to write
	 */
    if (buffer == NULL)
        return -1;
    conn_t *headPtr = pool->conn_head;
    msg_t *msgToAdd;
    for (; headPtr != NULL; headPtr = headPtr->next) {
        if (headPtr->fd == sd)
            continue;
        //creat new msg_t and init its param and allocate the message at the length we want.
        msgToAdd = (msg_t *) malloc(sizeof(msg_t));
        msgToAdd->next = NULL;
        msgToAdd->prev = NULL;
        msgToAdd->size = len;
        msgToAdd->message = (char *) malloc((len + 1) * sizeof(char));
        memset(msgToAdd->message, '\0', len + 1);//fill with '\0' and copy data
        strcpy(msgToAdd->message, buffer);
        //now i check if the msg_head is null, if so i define all fields.
        if (headPtr->write_msg_head == NULL) {
            headPtr->write_msg_head = msgToAdd;
            headPtr->write_msg_tail = msgToAdd;
            headPtr->write_msg_head->next = headPtr->write_msg_tail;
            headPtr->write_msg_tail->prev = headPtr->write_msg_head;
            FD_SET(headPtr->fd, &pool->write_set);//turn on write and continue;
            continue;
        }//if isn't null
        msgToAdd->prev = headPtr->write_msg_tail;
        headPtr->write_msg_tail->next = msgToAdd;
        headPtr->write_msg_tail = msgToAdd;
        FD_SET(headPtr->fd, &pool->write_set);
    }
    return 0;
}

int write_to_client(int sd, conn_pool_t *pool) {
    /*
	 * 1. write all msgs in queue
	 * 2. deallocate each writen msg
	 * 3. if all msgs were writen successfully, there is nothing else to write to this fd...
     * */

    conn_t *connPtr = pool->conn_head;//here i catch the rigth fd in the connections on the pool
    for (; connPtr != NULL; connPtr = connPtr->next) {
        if (connPtr->fd == sd)
            break;
    }
    msg_t *msgT = connPtr->write_msg_head;
    if (connPtr->write_msg_head == connPtr->write_msg_tail) {
        //this is the same msg!
        connPtr->write_msg_tail = NULL;
        connPtr->write_msg_head = NULL;
        FD_CLR(sd, &pool->write_set);
    } else {
        //points the head to the next one and the last to NULL
        connPtr->write_msg_head = connPtr->write_msg_head->next;
        connPtr->write_msg_head->prev = NULL;
    }
    write(sd, msgT->message, msgT->size);
    //free msg himself, then the msg to avoid mem leak.
    free(msgT->message);
    free(msgT);
    return 0;
}