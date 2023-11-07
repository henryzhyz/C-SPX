1. Describe how your exchange works.

Firstlly, set up the signal handller using function ---- void sig_recv(int sigNum, siginfo_t *sigInfo, void *context)
Inside the signal handler, it will proccess the data from trader_a, trader_b,........., handel the input data like "sell" "buy" "cancel" and ""amend"..
Also, it will print all the info to std

After setting up the signal, it will init the trader list by handling the commandline input 
Then for each trader it will create two pipes one for send and one for receive

Then for  each trader, it will fork() once, and inside the child process, it will use execl() to process trader program with input argument including "trader_a" binary file and trader_id, and the parent  process will connect the pipes and record the trader pid and mark as online as a flag to disconnect a trader. 

After all traders disconnect, the whole program will exist. 


2. Describe your design decisions for the trader and how it's fault-tolerant.

trader.c is similar to exchange.c setup, firstly set up the siganl handelr and sigaction via function void sig_recv(int sigNum,siginfo_t* sigInfo,void* context), inside the function, it will handle the input by reading from the the data send by its pipelines. For example, it will auto send "BUY ....." to exhange once it receive "SELL ...". After setting up the signal, it will create two pipes and open the rece pipeline for reading data. Then fianlly, it will check if it can open the send pipeline to make sure that the trader is disconnect or not. The fault toleraant including the invalid message handle and also check the signaction return value, also the sigNum correctness. 

3. Describe your tests and how to run them.
I have written unit_tests.c files but its not compile with the cmocka.h file. Basically, it will check the connection of the two pipes, check if can read and response the messegae between exchange and trader properly. 