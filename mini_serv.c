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
}           t_client;

t_client *clients = NULL;
fd_set read_sock, write_sock, cur_sock;
char msg[200000], buff[200020];
int serv_sock, next_id;

void    fatal_error()
{
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    close(serv_sock);
    exit(1);
}

int maxSock()
{
    t_client *tmp =  clients;
    int max = serv_sock;

    while(tmp)
    {
        if (tmp->fd > max)
            max = tmp->fd;
        tmp = tmp->next;
    }
    return max;
}

int getNextId(int fd)
{
    t_client *tmp =  clients;
   while(tmp)
    {
        if (tmp->fd == fd)
            return tmp->id;
        tmp = tmp->next;
    }
    return -1;
}

void    send2All(int fd)
{
    t_client *tmp =  clients;

    while(tmp)
    {
        if (tmp->fd != fd && FD_ISSET(tmp->fd, &write_sock))
        {
            if (send(tmp->fd, buff, strlen(buff), 0) == -1)
                fatal_error();
        }
        tmp = tmp->next;
    }
}

void    add_client()
{
    t_client *tmp =  clients;
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
    sprintf(buff, "server: client %d just arrived\n", next_id);
    send2All(client_fd);

    new->fd = client_fd;
    new->id = next_id++;
    new->next = NULL;

    if (!clients)
        clients = new;
    else
    {
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = new;
    }
}

void    rmv_client(int fd)
{
    t_client *prev = NULL, *tmp = clients;

    bzero(&buff, sizeof(buff));
    sprintf(buff, "server: client %d just left\n", getNextId(fd));
    send2All(fd);

    while(tmp)
    {
        if (tmp->fd == fd)
        {
            if (prev)
                prev->next = tmp->next;
            else
                clients = tmp->next;
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
    char tmp[200000];

    while(msg[++i])
    {
        tmp[++j] = msg[i];
        if (msg[i] == '\n')
        {
            bzero(&buff, sizeof(buff));
            sprintf(buff, "client %d: %s", getNextId(fd), tmp);
            send2All(fd);
            bzero(&tmp, sizeof(tmp));
            j = -1;
        }

    }
    bzero(&msg, sizeof(msg));
}

void    servSockInit(char *port)
{
	struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
	servaddr.sin_port = htons(atoi(port)); 

	serv_sock = socket(AF_INET, SOCK_STREAM, 0); 
	if (serv_sock == -1)
		fatal_error();
	if ((bind(serv_sock, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
		fatal_error();
	if (listen(serv_sock, 128) != 0)
		fatal_error();

    FD_ZERO(&cur_sock);
    FD_SET(serv_sock, &cur_sock);
    bzero(&msg, sizeof(msg));
}

int main(int ac, char **av) {

    if (ac != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    servSockInit(av[1]);

    while (1)
    {
        read_sock = write_sock = cur_sock;
        if (select(maxSock() + 1, &read_sock, &write_sock, NULL, NULL) < 0)
            continue;

        for (int socket = 0; socket <= maxSock(); ++socket)
        {
            // if (FD_ISSET(socket, &read_sock))
            // {
			// 	if (socket == serv_sock)
			// 	{
			// 		add_client();
			// 		break ;
			// 	}
			// 	int	ret = 1;
			// 	while (ret == 1 && msg[strlen(msg) - 1] != '\n')
			// 	{
			// 		ret = recv(socket, msg + strlen(msg), 1, 0);
			// 		if (ret <= 0)
			// 			break ;
			// 	}
			// 	if (ret <= 0)
			// 	{
			// 		rmv_client(socket);
			// 		break ;
			// 	}
			// 	process_msg(socket);
			// }

                if (socket == serv_sock)
                {
                    add_client();
                    break;
                }
                int bytes = recv(socket, msg + strlen(msg), sizeof(msg) - strlen(msg), 0);
                    if (bytes <= 0)
                    {
                        rmv_client(socket);
                        break;
                    }
                    else if (msg[strlen(msg) - 1] == '\n')
                        process_msg(socket);
                }
            }
        }
	return 0;
}