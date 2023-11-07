
#define _POSIX_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include "spx_common.h"
#include "cmocka.h"

int m_trader_id = -1;
int m_recv_fd = -1;
int m_order_id = 0;
char m_send_fifo_name[LINE_SIZE];
int m_exchange_pid = -1;

static void order_buy_test(void **state) {
    (void) state;
	char reply_msg[32] = {0};
    char send_msg[32] = {0};
	char check_msg[32] = {0};
	sprintf(send_msg,"BUY %d GPU 3 500;",m_order_id);
	m_order_id++;
	int fd = open(m_send_fifo_name,O_WRONLY | O_NONBLOCK);
	write(fd,send_msg,strlen(send_msg));
	kill(m_exchange_pid,SIGUSR1);
	int count = 1;
	do
	{
		count++;
		int len = read(m_recv_fd,reply_msg,32);
		if(len>0)
			break;
		sleep(1);
	}while(count>3);
	sprintf(check_msg, "ACCEPTED %s;", m_order_id);
    assert_true(strcmp(reply_msg,check_msg));
}

static void order_sell_test(void **state) {
    (void) state;
	char reply_msg[32] = {0};
    char send_msg[32] = {0};
	char check_msg[32] = {0};
	sprintf(send_msg,"SELL %d GPU 3 500;",m_order_id);
	m_order_id++;
	int fd = open(m_send_fifo_name,O_WRONLY | O_NONBLOCK);
	write(fd,send_msg,strlen(send_msg));
	kill(m_exchange_pid,SIGUSR1);
	int count = 1;
	do
	{
		count++;
		int len = read(m_recv_fd,reply_msg,32);
		if(len>0)
			break;
		sleep(1);
	}while(count>3);
	sprintf(check_msg, "ACCEPTED %s;", m_order_id);
    assert_true(strcmp(reply_msg,check_msg));
}

static void order_sell_test(void **state) {
    (void) state;
	char reply_msg[32] = {0};
    char send_msg[32] = {0};
	char check_msg[32] = {0};
	sprintf(send_msg,"SELL %d GPU 3 500;",m_order_id);
	m_order_id++;
	int fd = open(m_send_fifo_name,O_WRONLY | O_NONBLOCK);
	write(fd,send_msg,strlen(send_msg));
	kill(m_exchange_pid,SIGUSR1);
	int count = 1;
	do
	{
		count++;
		int len = read(m_recv_fd,reply_msg,32);
		if(len>0)
			break;
		sleep(1);
	}while(count>3);
	sprintf(check_msg, "ACCEPTED %s;", m_order_id);
    assert_true(strcmp(reply_msg,check_msg));
}


static void order_amend_test(void **state) {
    (void) state;
	char reply_msg[32] = {0};
    char send_msg[32] = {0};
	char check_msg[32] = {0};
	sprintf(send_msg,"AMEND %d 3 500;",m_order_id);
	m_order_id++;
	int fd = open(m_send_fifo_name,O_WRONLY | O_NONBLOCK);
	write(fd,send_msg,strlen(send_msg));
	kill(m_exchange_pid,SIGUSR1);
	int count = 1;
	do
	{
		count++;
		int len = read(m_recv_fd,reply_msg,32);
		if(len>0)
			break;
		sleep(1);
	}while(count>3);
	sprintf(check_msg, "AMENDED %s;", m_order_id);
    assert_true(strcmp(reply_msg,check_msg));
}

static void order_cancel_test(void **state) {
    (void) state;
	char reply_msg[32] = {0};
    char send_msg[32] = {0};
	char check_msg[32] = {0};
	sprintf(send_msg,"CANCEL %d;",m_order_id);
	m_order_id++;
	int fd = open(m_send_fifo_name,O_WRONLY | O_NONBLOCK);
	write(fd,send_msg,strlen(send_msg));
	kill(m_exchange_pid,SIGUSR1);
	int count = 1;
	do
	{
		count++;
		int len = read(m_recv_fd,reply_msg,32);
		if(len>0)
			break;
		sleep(1);
	}while(count>3);
	sprintf(check_msg, "CANCELLED %s;", m_order_id);
    assert_true(strcmp(reply_msg,check_msg));
}



void sig_recv(int sigId,siginfo_t* sigInfo,void* context)
{
	if(sigId==SIGUSR1 && m_exchange_pid==-1)
	{
		char buf[BUF_SIZE] = {0};
		int recv_len = 0;
		int len = read(m_recv_fd,buf,1024);
		while(len>0){
			recv_len += len;
			len = read(m_recv_fd,buf+recv_len,1024);
		}
		if(recv_len>0){
			
			int msg_count = 0;
			char msg_lines[1024][LINE_SIZE] = {{0}};
			char *msg_line = strtok(buf,";");
			while(msg_line){
				strcpy(msg_lines[msg_count],msg_line);
				msg_count++;
				msg_line = strtok(NULL,";");
			}
			
			for(int i=0;i<msg_count;i++)
			{			
				int word_count = 0;
				char words[5][LINE_SIZE] = {{0}};
				char *word = strtok(msg_lines[i]," ");
				while(word)
				{
					strcpy(words[word_count],word);
					word_count++;
					word=strtok(NULL," ");
				}
				if(strcmp(words[1],"OPEN")==0)  
				{
					m_exchange_pid = sigInfo->si_pid;
					break;
				}
			}
		}
	}
}

int main(int argc, char ** argv) 
{
	if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }
	m_trader_id = atoi(argv[1]);
    // register signal handler
	struct sigaction sigact;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags=SA_SIGINFO;
	sigact.sa_handler=(__sighandler_t)sig_recv; 
	if(sigaction(SIGUSR1,&sigact,NULL)==-1){
		printf("signal init error\n");
		return -1;
	}
	
    const struct CMUnitTest tests[8] = {
        cmocka_unit_test(order_buy_test),
		cmocka_unit_test(order_sell_test),
		cmocka_unit_test(order_cancel_test),
		cmocka_unit_test(order_amend_test)
    };
		
	char fifo_exchange_name[32];
	memset(fifo_exchange_name,0,32);
	memset(m_send_fifo_name,0,32);
	sprintf(fifo_exchange_name,FIFO_EXCHANGE,m_trader_id);
	sprintf(m_send_fifo_name,FIFO_TRADER,m_trader_id);
	m_recv_fd = open(fifo_exchange_name, O_RDONLY | O_NONBLOCK);

    return cmocka_run_group_tests(tests, NULL, NULL);
} 

