#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <signal.h>

int		count = 0, max_fd = 0; // счетчик ID клиентов, максимальный файловый дескриптор
int		ids[1024]; // массив для связи дескрипторов с ID клиентов
char	*msgs[1024]; // буферы сообщений клиентов

fd_set	rfds, wfds, afds; // наборы дескрипторов для чтения/записи/активных соединений
int		sockfd = 0;
char	buf_read[1001], buf_write[128]; // буфер для входящих сообщений, для форматированных сообщений сервера

// START COPY-PASTE FROM GIVEN MAIN

// Извлекает одно сообщение (до \n) из буфера клиента. 
// Выделяет память для остатка буфера, обновляет указатели и возвращает 1 при успехе,
// 0 если нет \n, или -1 при ошибке выделения памяти.

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

// Объединяет текущий буфер клиента с новым входящим сообщением.
// Выделяет память для новой строки, копирует старый буфер (если есть)
// и добавляет новое сообщение, освобождая старый буфер.

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

// END COPY-PASTE

// Выводит "Fatal error" в stderr и
// завершает программу с кодом 1 при критической ошибке.

void	fatal_error()
{
	write(2, "Fatal error\n", 13);
	exit(1);
}

// Отправляет сообщение всем клиентам (кроме автора),
// которые готовы к записи (проверяется через wfds).

void	notify_other(int author, char *str)
{
	for (int fd = 0; fd <= max_fd; fd++)
	{
		if (FD_ISSET(fd, &wfds) && fd != author)
			send(fd, str, strlen(str), 0);
	}
}

// Регистрирует нового клиента: обновляет max_fd, присваивает ID,
// инициализирует буфер сообщений, добавляет дескриптор в afds.
// Оповещает других о подключении клиента.

void	register_client(int fd)
{
	max_fd = fd > max_fd ? fd : max_fd;
	ids[fd] = count++;
	msgs[fd] = NULL;
	FD_SET(fd, &afds);
	sprintf(buf_write, "server: client %d just arrived\n", ids[fd]);
	notify_other(fd, buf_write);
}

// Удаляет клиента: оповещает других об отключении, освобождает буфер
// сообщений, убирает дескриптор из afds и закрывает соединение.

void	remove_client(int fd)
{
	sprintf(buf_write, "server: client %d just left\n", ids[fd]);
	notify_other(fd, buf_write);
	free(msgs[fd]);
	FD_CLR(fd, &afds);
	close(fd);
}

void handler(int sig) {
    if (sig == SIGINT) {
        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &afds) && fd != sockfd) {
                remove_client(fd);
            }
        }
        close(sockfd);
        exit(0);
    }
}

// Извлекает сообщения из буфера клиента (через extract_message), форматирует их
// с префиксом "client <ID>: " и рассылает всем, кроме отправителя.

void	send_msg(int fd)
{
	char *msg;

	while (extract_message(&(msgs[fd]), &msg))
	{
		sprintf(buf_write, "client %d: ", ids[fd]);
		notify_other(fd, buf_write);
		notify_other(fd, msg);
		free(msg);
	}
}

// Проверяет наличие аргумента (порт). Создает сокет, настраивает
// адрес сервера (127.0.0.1, порт из аргумента),
// привязывает (bind) и переводит в режим прослушивания (listen).
// В бесконечном цикле:
// Копирует активные дескрипторы в rfds/wfds и вызывает select для проверки готовности.
// Для каждого готового дескриптора:
// Если это серверный сокет, принимает новое соединение (accept) и регистрирует клиента.
// Если это клиент, читает данные (recv). Если данных нет или ошибка,
// удаляет клиента. Иначе добавляет данные в буфер и рассылает сообщения.

int		main(int ac, char **av)
{
	signal(SIGINT, handler);
	signal(SIGPIPE, handler);

	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	FD_ZERO(&afds);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    	if (sockfd < 0)
		fatal_error();
    	max_fd = sockfd; // Явно присваиваем max_fd
    	FD_SET(sockfd, &afds);

	// START COPY-PASTE FROM MAIN

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	servaddr.sin_port = htons(atoi(av[1])); // replace 8080

	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)))
		fatal_error();
	if (listen(sockfd, SOMAXCONN)) // the main uses 10, SOMAXCONN is 180 on my machine
		fatal_error();

	// END COPY-PASTE

	while (1)
	{
		rfds = wfds = afds;

		if (select(max_fd + 1, &rfds, &wfds, NULL, NULL) < 0)
			fatal_error();

		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (!FD_ISSET(fd, &rfds))
				continue;

			if (fd == sockfd)
			{
				socklen_t addr_len = sizeof(servaddr);
				int client_fd = accept(sockfd, (struct sockaddr *)&servaddr, &addr_len);
				if (client_fd >= 0)
				{
					register_client(client_fd);
					break ;
				}
			}
			else
			{
				int read_bytes = recv(fd, buf_read, 1000, 0);
				if (read_bytes <= 0)
				{
					remove_client(fd);
					break ;
				}
				buf_read[read_bytes] = '\0';
				msgs[fd] = str_join(msgs[fd], buf_read);
				send_msg(fd);
			}
		}
	}
	return 0;
}

