#include "spx_trader.h"


int m_trader_id = -1;
int m_recv_fd = -1;
int m_order_id = 0;
char m_send_fifo_name[LINE_SIZE];

//signal handller
void sig_recv(int sigNum, siginfo_t *sigInfo, void *context)
{
	if (sigNum == SIGUSR1)
	{
		char buf[BUF_SIZE] = {0};
		int recv_len = 0;
		int len = read(m_recv_fd, buf, 1024);
		//make sure can read the pipes
		while (len > 0)
		{
			recv_len += len;
			len = read(m_recv_fd, buf + recv_len, 1024);
		}
		//handle the input
		if (recv_len > 0)
		{ // make sure there are data inside the buffer
			int msg_count = 0;
			char msg_lines[1024][LINE_SIZE] = {{0}};
			char *msg_line = strtok(buf, ";");
			while (msg_line)
			{
				strcpy(msg_lines[msg_count], msg_line);
				msg_count++;
				msg_line = strtok(NULL, ";");
			}
			//AT tradechar 
			for (int i = 0; i < msg_count; i++)
			{
				//printf("[PEX-Milestone] Exchange -> Trader: %s\n", msg_lines[i]);
				int word_count = 0;
				char words[5][NAME_SIZE] = {{0}};
				char *word = strtok(msg_lines[i], " ");
				while (word)
				{
					strcpy(words[word_count], word);
					word_count++;
					word = strtok(NULL, " ");
				}

				if (strcmp(words[1], "SELL") == 0)
				{
					//trader disconnect if order quality over 1000
					if (atoi(words[3]) >= 1000)
					{
						close(m_recv_fd);
						return;
					}
					char send_msg[LINE_SIZE] = {0};
					sprintf(send_msg, "BUY %d %s %s %s;", m_order_id, words[2], words[3], words[4]);
					m_order_id++;
					int fd = open(m_send_fifo_name, O_WRONLY | O_NONBLOCK);
					write(fd, send_msg, strlen(send_msg));
					kill(sigInfo->si_pid, SIGUSR1);
					//printf("[PEX-Milestone] Trader -> Exchange: %s\n", send_msg);
				}
			}
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Not enough arguments\n");
		return 1;
	}
	//printf("[PEX-Milestone] Launching trader ./spx_trader\n");

	m_trader_id = atoi(argv[1]);
	// register signal handler
	
	struct sigaction sigact;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_SIGINFO;
	sigact.sa_handler = (__sighandler_t)sig_recv;
	if (sigaction(SIGUSR1, &sigact, NULL) == -1)
	{
		printf("signal init error\n");
		return -1;
	}



	// connect to named pipes
	char recv_fifo_name[LINE_SIZE];
	memset(recv_fifo_name, 0, LINE_SIZE);
	memset(m_send_fifo_name, 0, LINE_SIZE);

	sprintf(recv_fifo_name, FIFO_EXCHANGE, m_trader_id);
	sprintf(m_send_fifo_name, FIFO_TRADER, m_trader_id);
	m_recv_fd = open(recv_fifo_name, O_RDONLY | O_NONBLOCK);
	//printf("[PEX-Milestone] Opened Named Pipes\n");

	// event loop:
	while (1)
	{
		
		sleep(1);
		int fd = open(m_send_fifo_name, O_WRONLY | O_NONBLOCK);
		if (fd < 0)
		{
			//printf("[PEX-Milestone] Trader disconnected\n");
			break;
		}
	}
}