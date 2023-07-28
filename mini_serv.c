#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct s_client
{
    int fd;
    int id;
    struct s_client *next;
}       t_client;

t_client *clients = NULL;
int serv_sock, cli_id;
fd_set read_sock, write_sock, cur_sock;
char msg[200000], buff[200030];

void    fatal_error() {
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    close(serv_sock);
    exit(1);
}

int max_sock()
{
    t_client *tmp = clients;
    int max = serv_sock;

    while (tmp)
    {
        if (tmp->fd > max)
            max = tmp->fd;
        tmp = tmp->next;
    }
    return max;
}

int get_client_id(int fd)
{
    t_client *tmp = clients;
    while (tmp)
    {
        if (tmp->fd == fd)
            return tmp->id;
        tmp = tmp->next;
    }
    return -1;
}

void    send_2_all(int fd)
{
    t_client *tmp = clients;

    while (tmp)
    {
        if (tmp->fd != fd && FD_ISSET(tmp->fd, &write_sock))
        {
            if (send(tmp->fd, buff, strlen(buff), 0) == -1)
                fatal_error();
        }
        tmp = tmp->next;
    }
}

void    add_new_client()
{
    t_client *tmp = clients;
    t_client *new =  malloc(sizeof(t_client));
    if (!new)
        fatal_error();
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(serv_sock, (struct sockaddr *)&client_addr, &len);
    if (client_fd == -1)
        fatal_error();

    FD_SET(client_fd, &cur_sock);
    bzero(&buff, sizeof(buff));
    sprintf(buff, "server: client %d just arrived\n", cli_id);
    send_2_all(client_fd);

    new->fd = client_fd;
    new->id = cli_id++;
    new->next = NULL;

    if (!clients)
        clients = new;
    else
    {
        while(tmp->next)
            tmp = tmp->next;
        tmp->next = new;
    }
}

void    remove_client(int fd)
{
    t_client *tmp = clients, *prev = NULL;

    bzero(&buff, sizeof(buff));
    sprintf(buff, "server: client %d just left\n", cli_id);
    send_2_all(fd);

    while (tmp)
    {
        if (tmp->fd == fd)
        {
            if (prev)
                prev->next = tmp->next;
            else
                clients =  tmp->next;
            free(tmp);
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    FD_CLR(fd, &cur_sock);
    close(fd);
}

void    process_msg(int fd)
{
    int i = -1, j = -1;
    char line[200000];

    while(msg[++i])
    {
        line[++j] = msg[i];
        if (msg[i] == '\n')
        {
            bzero(&buff, sizeof(buff));
            sprintf(buff, "client %d: %s", get_client_id(fd), line);
            send_2_all(fd);
            bzero(&line, sizeof(line));
            j = -1;
        }
    }
    bzero(&msg, sizeof(msg));
}

void    serv_sock_init(char *port)
{
	struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(port));  

	serv_sock = socket(AF_INET, SOCK_STREAM, 0); 
	if (serv_sock == -1)
        fatal_error();
	if ((bind(serv_sock, (struct sockaddr *)&servaddr, sizeof(servaddr))) == -1)
		fatal_error();
	if (listen(serv_sock, 128) == -1)
		fatal_error();

    FD_ZERO(&cur_sock);
    FD_SET(serv_sock, &cur_sock);
    bzero(&msg, sizeof(msg));

	// len = sizeof(cli);
	// connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	// if (connfd < 0) { 
    //     printf("server acccept failed...\n"); 
    //     exit(0); 
    // } 
    // else
    //     printf("server acccept the client...\n");
}

int main(int ac, char **av)
{
	if (ac != 2){
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    serv_sock_init(av[1]);

    while (1)
    {
        read_sock = write_sock = cur_sock;
        if (select(max_sock() + 1, &read_sock, &write_sock, NULL, NULL) == -1)
            continue;
        for (int socket = 0; socket <= max_sock(); ++socket)
        {
            if (FD_ISSET(socket, &read_sock))
            {
                if (socket == serv_sock)
                {
                    add_new_client();
                    break;
                }
                else 
                {
                    int bytes = recv(socket, msg + strlen(msg), sizeof(msg) - strlen(msg)  - 1, 0);
                    if (bytes <= 0)
                    {
                        remove_client(socket);
                        break;
                    }
                    else if (msg[strlen(msg) - 1] == '\n')
                        process_msg(socket);
                }
            }
        }
    }
    return 0;
}