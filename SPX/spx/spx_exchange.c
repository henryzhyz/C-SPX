/**
 * comp2017 - assignment 3
 * HONGYI Zhang
 * hzha4450
 */

#include "spx_exchange.h"

long long total_free_amount = 0; //total transaction fee
enum OrderType
{
	BUY = 0,
	SELL
};
ProductBook *m_product_list = NULL;
struct Trader *m_trader_list = NULL;
int m_trader_count = 0;
bool m_is_match = true;

//notify all trader Market is open
void notify_all_trader()
{
	Trader *trader_node = m_trader_list;
	char *cmd = "MARKET OPEN;";

	while (trader_node != NULL)
	{
		int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
		write(fd, cmd, strlen(cmd));
		kill(trader_node->trader_pid, SIGUSR1);
		trader_node = trader_node->next;
	}
}

//read the info from products.txt file
int get_product_from_file(char *file_name)
{
	if (file_name == NULL)
		return -1;
	FILE *p_file = fopen(file_name, "r");
	if (!p_file)
	{
		printf("file open error\n");
		return -1;
	}
	char buf[BUF_SIZE] = {0};
	int count = 0;

	if (fgets(buf, BUF_SIZE, p_file))
	{
		count = atoi(buf);
	}
	else
	{
		return -1;
	}

	ProductBook *product_node;
	ProductBook *temp_product_node;
	memset(buf, 0, BUF_SIZE);
	//read line by line
	while (fgets(buf, BUF_SIZE, p_file))
	{
		//init the ProductBook list
		product_node = (ProductBook *)malloc(sizeof(ProductBook));
		product_node->sell_order_list = NULL;
		product_node->buy_order_list = NULL;
		product_node->next = NULL;
		memset(product_node->product_name, 0, NAME_SIZE);
		memcpy(product_node->product_name, buf, strlen(buf) - 1);
		//add at the head of the list
		if (m_product_list == NULL)
		{
			m_product_list = product_node;
			temp_product_node = product_node;
		}
		else
		{
			temp_product_node->next = product_node;
			temp_product_node = product_node;
		}
		memset(buf, 0, BUF_SIZE);
	}
	return count;
}

//free all the dynamics memory and unlink the pipelines
void clean()
{
	Trader *trader_node = m_trader_list;
	while (m_trader_list != NULL)
	{
		//free the TradeInfo List inside the Trader struct
		TradeInfo *trade_node = m_trader_list->trade_data;
		while (m_trader_list->trade_data != NULL)
		{
			m_trader_list->trade_data = m_trader_list->trade_data->next;
			free(trade_node);
			trade_node = m_trader_list->trade_data;
		}
		//free trader and the pipeline asscosiated with this specific trader
		m_trader_list = m_trader_list->next;
		unlink(trader_node->fifo_control.send_fifo_name);
		unlink(trader_node->fifo_control.recv_fifo_name);
		free(trader_node);
		trader_node = m_trader_list;
	}

	//free ProductBook (SellOrder, same_price SellOrder, BuyOrder, same_price BuyOrder)
	ProductBook *product_node = m_product_list;
	while (m_product_list != NULL)
	{
		//first free Sell same_price node list, then free SellOrder List
		SellOrder *sell_order_node = m_product_list->sell_order_list;
		while (m_product_list->sell_order_list != NULL)
		{
			m_product_list->sell_order_list = m_product_list->sell_order_list->next;
			SellOrder *same_node = sell_order_node->same_price_order;
			while (sell_order_node->same_price_order != NULL)
			{
				sell_order_node->same_price_order = same_node->same_price_order;
				free(same_node);
				same_node = sell_order_node->same_price_order;
			}
			free(sell_order_node);
			sell_order_node = m_product_list->sell_order_list;
		}
		//first free Buy same_price node list, then free BuyOrder List
		BuyOrder *buy_order_node = m_product_list->buy_order_list;
		while (m_product_list->buy_order_list != NULL)
		{
			m_product_list->buy_order_list = m_product_list->buy_order_list->next;
			BuyOrder *same_node = buy_order_node->same_price_order;
			while (buy_order_node->same_price_order != NULL)
			{
				buy_order_node->same_price_order = same_node->same_price_order;
				free(same_node);
				same_node = buy_order_node->same_price_order;
			}
			free(buy_order_node);
			buy_order_node = m_product_list->buy_order_list;
		}
		//free product node
		m_product_list = m_product_list->next;
		free(product_node);
		product_node = m_product_list;
	}
}

//update the tarde_info of each trader of one traded product after one match
void update_trade_data(int trader_id, char *product_name, int quality, long long amount, int order_id)
{
	Trader *trader_node = m_trader_list;
	while (trader_node != NULL)
	{
		if (trader_node->trader_id == trader_id)
		{
			TradeInfo *trade_node = trader_node->trade_data;
			while (trade_node != NULL)
			{
				if (strcmp(trade_node->product_name, product_name) == 0)
				{
					trade_node->total_quality += quality;
					trade_node->total_amount += amount;

					if (quality < 0)
						quality *= -1;
					char cmd[LINE_SIZE] = {0};
					sprintf(cmd, "FILL %d %d;", order_id, quality);
					int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
					write(fd, cmd, strlen(cmd));
					kill(trader_node->trader_pid, SIGUSR1);

					break;
				}
				trade_node = trade_node->next;
			}
			break;
		}
		trader_node = trader_node->next;
	}
}

//search and get the product book
ProductBook *get_product_info(char *product_name)
{
	ProductBook *product_node = m_product_list;
	while (product_node != NULL)
	{
		if (strcmp(product_node->product_name, product_name) == 0)
			return product_node;
		product_node = product_node->next;
	}
	return NULL;
}

//Match the trade
int trade_match(int trader_id, int order_id, int order_type, char *product_name, int quality, int price)
{

	ProductBook *product_node = get_product_info(product_name);
	//product does not exist
	if (!product_node)
		return -1;
	//the return value of the  trade number of product LEFT after one match
	int total_quality = quality;
	//to match the sell_order_list
	if (order_type == BUY)
	{
		SellOrder *sell_order_node = product_node->sell_order_list;
		SellOrder *temp_node = sell_order_node;
		while (sell_order_node != NULL)
		{
			//buy price must >= sell price
			if (sell_order_node->product_price <= price)
			{

				int filled = 0; //check if the buy_number <= sell_number, if less then filled
				long long amount = 0;
				long long free_amount = 0; //transaction fee
				int match_quality = 0;	   //trade number of product
				if (sell_order_node->product_quality > quality)
				{
					sell_order_node->product_quality -= quality;
					match_quality = quality;
					amount = (long long)sell_order_node->product_price * quality;
					free_amount = amount / 100.0 + 0.5;
					total_free_amount += free_amount;
				}
				//buy order filled
				else
				{
					filled = 1;
					quality -= sell_order_node->product_quality;
					match_quality = sell_order_node->product_quality;
					amount = (long long)sell_order_node->product_price * sell_order_node->product_quality;
					free_amount = amount / 100.0 + 0.5;
					total_free_amount += free_amount;
				}
				total_quality -= match_quality;

				//update each sell aand buy trader info
				update_trade_data(trader_id, product_name, match_quality, amount * -1 - free_amount, order_id);
				update_trade_data(sell_order_node->trader_id, product_name, match_quality * -1, amount, sell_order_node->order_id);

				printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%lld, fee: $%lld.\n",
					   LOG_PREFIX,
					   sell_order_node->order_id,
					   sell_order_node->trader_id,
					   order_id,
					   trader_id,
					   amount,
					   free_amount);
				//in case sold out, free the sell_order node in sell_order_list
				if (filled == 1)
				{
					//first check if its a same_price node
					if (sell_order_node->same_price_order != NULL)
					{
						SellOrder *temp = sell_order_node->same_price_order;
						temp->next = sell_order_node->next;
						//free sell_order_list head
						if (temp_node == product_node->sell_order_list)
						{
							product_node->sell_order_list = temp;
							free(sell_order_node);
							sell_order_node = product_node->sell_order_list;
							temp_node = sell_order_node;
						}
						//sell_order_node at middle
						else
						{
							temp_node->next = temp;
							free(sell_order_node);
							sell_order_node = temp;
						}
					}
					else
					{
						//sell_order_node at head
						if (temp_node == product_node->sell_order_list)
						{
							product_node->sell_order_list = temp_node->next;
							free(sell_order_node);
							sell_order_node = product_node->sell_order_list;
							temp_node = sell_order_node;
						}
						//sell_order_node at middle
						else
						{
							temp_node->next = sell_order_node->next;
							free(sell_order_node);
							sell_order_node = temp_node->next;
						}
					}
					continue;
				}
			}
			temp_node = sell_order_node;
			sell_order_node = sell_order_node->next;
		}
	}
	else
	{
		//match the buy order same as the above
		BuyOrder *buy_order_node = product_node->buy_order_list;
		BuyOrder *temp_node = buy_order_node;
		while (buy_order_node != NULL)
		{
			if (buy_order_node->product_price >= price)
			{
				int filled = 0;
				long long amount = 0;
				long long free_amount = 0;
				int match_quality = 0;
				if (buy_order_node->product_quality > quality)
				{
					buy_order_node->product_quality -= quality;
					match_quality = quality;
					amount = (long long)buy_order_node->product_price * quality;
					free_amount = amount / 100.0 + 0.5;
					total_free_amount += free_amount;
				}
				else
				{
					filled = 1;
					quality -= buy_order_node->product_quality;
					match_quality = buy_order_node->product_quality;
					amount = (long long)buy_order_node->product_price * buy_order_node->product_quality;
					free_amount = amount / 100.0 + 0.5;
					total_free_amount += free_amount;
				}

				total_quality -= match_quality;
				//update the trader info for seller and buyer
				update_trade_data(trader_id, product_name, match_quality * -1, amount - free_amount, order_id);
				update_trade_data(buy_order_node->trader_id, product_name, match_quality, amount * -1, buy_order_node->order_id);

				printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%lld, fee: $%lld.\n",
					   LOG_PREFIX,
					   buy_order_node->order_id,
					   buy_order_node->trader_id,
					   order_id,
					   trader_id,
					   amount,
					   free_amount);

				if (filled == 1)
				{

					if (buy_order_node->same_price_order != NULL)
					{
						BuyOrder *temp = buy_order_node->same_price_order;
						temp->next = buy_order_node->next;

						if (temp_node == product_node->buy_order_list)
						{
							product_node->buy_order_list = temp;
							free(buy_order_node);
							buy_order_node = product_node->buy_order_list;
							temp_node = buy_order_node;
						}
						else
						{
							temp_node->next = temp;
							free(buy_order_node);
							buy_order_node = temp;
						}
					}
					else
					{
						if (temp_node == product_node->buy_order_list)
						{
							product_node->buy_order_list = temp_node->next;
							free(buy_order_node);
							buy_order_node = product_node->buy_order_list;
							temp_node = buy_order_node;
						}
						else
						{
							temp_node->next = buy_order_node->next;
							free(buy_order_node);
							buy_order_node = temp_node->next;
						}
					}
					continue;
				}
			}
			temp_node = buy_order_node;
			buy_order_node = buy_order_node->next;
		}
	}
	//left number of product
	return total_quality;
}

//add buy order to the product book buy_order list
void add_new_buy_order(BuyOrder *buy_order_node, char *product_name)
{
	ProductBook *product_node = get_product_info(product_name);
	BuyOrder *temp_node = product_node->buy_order_list;
	//buy_order list is empty
	if (temp_node == NULL)
	{
		product_node->buy_order_list = buy_order_node;
	}
	else
	{
		BuyOrder *temp = temp_node;
		while (temp_node != NULL)
		{
			//add to the same_price order
			if (temp_node->product_price == buy_order_node->product_price)
			{
				BuyOrder *same_node = temp_node->same_price_order;
				if (same_node == NULL)
				{
					temp_node->same_price_order = buy_order_node;
				}
				else
				{
					//add at the end of the list
					while (same_node->same_price_order != NULL)
						same_node = same_node->same_price_order;
					same_node->same_price_order = buy_order_node;
				}
				break;
			}
			else
			{
				//add price is bigger
				if (temp_node->product_price < buy_order_node->product_price)
				{
					//add at head
					if (temp_node == product_node->buy_order_list)
					{
						product_node->buy_order_list = buy_order_node;
						buy_order_node->next = temp_node;
					}
					//add at the end
					else
					{
						temp->next = buy_order_node;
						buy_order_node->next = temp_node;
					}
					break;
				}
			}
			temp = temp_node;
			temp_node = temp_node->next;
		}
	}
}

//add new sell order same logic as above buy function
void add_new_sell_order(SellOrder *sell_order_node, char *product_name)
{
	ProductBook *product_node = get_product_info(product_name);
	SellOrder *temp_node = product_node->sell_order_list;
	if (temp_node == NULL)
	{
		product_node->sell_order_list = sell_order_node;
	}
	else
	{
		SellOrder *temp = temp_node;
		while (temp_node != NULL)
		{
			if (temp_node->product_price == sell_order_node->product_price)
			{
				SellOrder *same_node = temp_node->same_price_order;
				if (same_node == NULL)
				{
					same_node->same_price_order = sell_order_node;
				}
				else
				{
					while (same_node->same_price_order != NULL)
						same_node = same_node->same_price_order;
					same_node->same_price_order = sell_order_node;
				}
				break;
			}
			else
			{
				if (temp_node->product_price < sell_order_node->product_price)
				{
					if (temp_node == product_node->sell_order_list)
					{
						product_node->sell_order_list = sell_order_node;
						sell_order_node->next = temp_node;
					}
					else
					{
						temp->next = sell_order_node;
						sell_order_node->next = temp_node;
					}
					break;
				}
			}
			temp = temp_node;
			temp_node = temp_node->next;
		}
	}
}

//exchange write to all other pipes(trader)
void notify_new_order(int trader_id, char *order_type, char *product_name, int quality, int price)
{
	char order_cmd[LINE_SIZE] = {0};
	sprintf(order_cmd, "MARKET %s %s %d %d;", order_type, product_name, quality, price);
	Trader *trader_node = m_trader_list;
	while (trader_node != NULL)
	{
		//in case not send yourself
		if (trader_node->trader_id != trader_id)
		{
			int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
			write(fd, order_cmd, strlen(order_cmd));
			kill(trader_node->trader_pid, SIGUSR1);
		}
		trader_node = trader_node->next;
	}
}

//after one match, if there are products left, needs to generate a new order
void product_buy_order(int trader_id, int order_id, char *product_name, int quality, int price)
{
	notify_new_order(trader_id, "BUY", product_name, quality, price);
	int total_quality = trade_match(trader_id, order_id, BUY, product_name, quality, price);
	if (total_quality > 0)
	{
		// generate a new buy order
		BuyOrder *buy_order_node = (BuyOrder *)malloc(sizeof(BuyOrder));
		buy_order_node->order_id = order_id;
		buy_order_node->trader_id = trader_id;
		buy_order_node->product_quality = total_quality;
		buy_order_node->product_price = price;
		buy_order_node->next = NULL;
		buy_order_node->same_price_order = NULL;
		add_new_buy_order(buy_order_node, product_name);
	}
}

//after one match, if there are products left, needs to generate a new order
void product_sell_order(int trader_id, int order_id, char *product_name, int quality, int price)
{
	notify_new_order(trader_id, "SELL", product_name, quality, price);
	int total_quality = trade_match(trader_id, order_id, SELL, product_name, quality, price);
	if (total_quality > 0)
	{
		//generate a new sell order
		SellOrder *sell_order_node = (SellOrder *)malloc(sizeof(SellOrder));
		sell_order_node->order_id = order_id;
		sell_order_node->trader_id = trader_id;
		sell_order_node->product_quality = total_quality;
		sell_order_node->product_price = price;
		sell_order_node->next = NULL;
		sell_order_node->same_price_order = NULL;
		add_new_sell_order(sell_order_node, product_name);
	}
}

int amend_product_order(int trader_id, int order_id, int quality, int price)
{
	int find = 0;
	char product_name[NAME_SIZE] = {0};
	ProductBook *product_node = m_product_list;
	while (product_node != NULL)
	{

		BuyOrder *buy_order_node = product_node->buy_order_list;
		BuyOrder *temp_buy_node = buy_order_node;
		while (buy_order_node != NULL)
		{
			if (buy_order_node->trader_id == trader_id && buy_order_node->order_id == order_id)
			{
				strcpy(product_name, product_node->product_name);
				//head
				if (temp_buy_node == product_node->buy_order_list)
				{
					if (temp_buy_node->same_price_order != NULL)
					{
						//get the temp_buy_node out (as a head of buy_order_list)
						product_node->buy_order_list = temp_buy_node->same_price_order;
						temp_buy_node->same_price_order->next = temp_buy_node->next;
					}
					else
					{
						//get the temp_buy_node out (as middle of buy_order_list)
						product_node->buy_order_list = temp_buy_node->next;
					}
					find = 1;
					break;
				}
				else
				{
					BuyOrder *free_node = buy_order_node;
					if (buy_order_node->same_price_order != NULL)
					{
						//buy_order_node and temp_buy_order node at the middle with a same_order list
						temp_buy_node->next = buy_order_node->same_price_order;
						buy_order_node->same_price_order->next = buy_order_node->next;
					}
					else
					{
						//without same_order list
						temp_buy_node->next = buy_order_node->next;
					}
					temp_buy_node = free_node;
					find = 1;
					break;
				}
			}
			//order at the same_order list buy not in the buy/sell order list
			else
			{
				BuyOrder *same_node = buy_order_node->same_price_order;
				BuyOrder *temp_same = same_node;
				while (same_node != NULL)
				{
					if (same_node->trader_id == trader_id && same_node->order_id == order_id)
					{
						strcpy(product_name, product_node->product_name);
						//at the head of the same_order list
						if (temp_same == buy_order_node->same_price_order)
						{
							buy_order_node->same_price_order = same_node->same_price_order; // same_node->same_price_order => same_node->next
						}

						else
						{
							temp_same->same_price_order = same_node->same_price_order; // same_node->same_price_order => same_node->next
						}
						//free the temp_buy_node later
						temp_buy_node = same_node;
						find = 1;
						break;
					}
					//keep tranverse
					temp_same = same_node;
					same_node = same_node->same_price_order;
				}
				if (find == 1)
					break;
			}
			temp_buy_node = buy_order_node;
			buy_order_node = buy_order_node->next;
		}

		if (find == 1)
		{
			product_buy_order(temp_buy_node->trader_id, temp_buy_node->order_id, product_name, quality, price);
			//find and free
			free(temp_buy_node);
			return 0;
		}

		SellOrder *sell_order_node = product_node->sell_order_list;
		SellOrder *temp_sell_node = sell_order_node;
		while (sell_order_node != NULL)
		{
			if (sell_order_node->trader_id == trader_id && sell_order_node->order_id == order_id)
			{
				strcpy(product_name, product_node->product_name);
				if (temp_sell_node == product_node->sell_order_list)
				{
					if (sell_order_node->same_price_order != NULL)
					{
						product_node->sell_order_list = sell_order_node->same_price_order;
						temp_sell_node->same_price_order->next = temp_sell_node->next;
					}
					else
					{
						product_node->sell_order_list = sell_order_node->next;
					}
					find = 1;
					break;
				}
				else
				{
					SellOrder *free_node = sell_order_node;
					if (sell_order_node->same_price_order != NULL)
					{
						temp_sell_node->next = sell_order_node->same_price_order;
					}
					else
					{
						temp_sell_node->next = sell_order_node->next;
					}
					temp_sell_node = free_node;
					find = 1;
					break;
				}
			}
			else
			{
				SellOrder *same_node = sell_order_node->same_price_order;
				SellOrder *temp_same = same_node;
				while (same_node != NULL)
				{
					if (same_node->trader_id == trader_id && same_node->order_id == order_id)
					{
						strcpy(product_name, product_node->product_name);
						if (temp_same == sell_order_node->same_price_order)
						{
							sell_order_node->same_price_order = same_node->same_price_order;
						}
						else
						{
							temp_same->same_price_order = same_node->same_price_order;
						}
						temp_sell_node = same_node;
						find = 1;
						break;
					}
					temp_same = same_node;
					same_node = same_node->same_price_order;
				}
			}
			temp_sell_node = sell_order_node;
			sell_order_node = sell_order_node->next;
		}
		if (find == 1)
		{
			product_sell_order(temp_sell_node->trader_id, temp_sell_node->order_id, product_name, quality, price);
			free(temp_sell_node);
			return 0;
		}
		product_node = product_node->next;
	}
	return -1;
}

//same tranverse as amend but imediate free after found
int cancel_product_order(int trader_id, int order_id, char *product_name, int *order_type)
{
	ProductBook *product_node = m_product_list;
	while (product_node != NULL)
	{
		BuyOrder *buy_order_node = product_node->buy_order_list;
		BuyOrder *temp_buy_node = buy_order_node;
		while (buy_order_node != NULL)
		{
			if (buy_order_node->trader_id == trader_id && buy_order_node->order_id == order_id)
			{
				*order_type = BUY; //a pointerr will be used for printing in the main
				strcpy(product_name, product_node->product_name);
				if (temp_buy_node == product_node->buy_order_list)
				{
					if (temp_buy_node->same_price_order != NULL)
					{
						product_node->buy_order_list = temp_buy_node->same_price_order;
						temp_buy_node->same_price_order->next = temp_buy_node->next;
					}
					else
					{
						product_node->buy_order_list = temp_buy_node->next;
					}
					free(temp_buy_node);
					return 0;
				}
				else
				{
					BuyOrder *free_node = buy_order_node;
					if (buy_order_node->same_price_order != NULL)
					{
						temp_buy_node->next = buy_order_node->same_price_order;
					}
					else
					{
						temp_buy_node->next = buy_order_node->next;
					}
					free(free_node);
					return 0;
				}
			}
			else
			{
				BuyOrder *same_node = buy_order_node->same_price_order;
				BuyOrder *temp_same = same_node;
				while (same_node != NULL)
				{
					if (same_node->trader_id == trader_id && same_node->order_id == order_id)
					{
						*order_type = BUY;
						strcpy(product_name, product_node->product_name);
						if (temp_same == buy_order_node->same_price_order)
						{
							buy_order_node->same_price_order = same_node->same_price_order;
						}
						else
						{
							temp_same->same_price_order = same_node->same_price_order;
						}
						free(same_node);
						return 0;
					}
					temp_same = same_node;
					same_node = same_node->same_price_order;
				}
			}
			temp_buy_node = buy_order_node;
			buy_order_node = buy_order_node->next;
		}

		SellOrder *sell_order_node = product_node->sell_order_list;
		SellOrder *temp_sell_node = sell_order_node;
		while (sell_order_node != NULL)
		{
			if (sell_order_node->trader_id == trader_id && sell_order_node->order_id == order_id)
			{
				*order_type = SELL;
				strcpy(product_name, product_node->product_name);
				if (temp_sell_node == product_node->sell_order_list)
				{
					if (sell_order_node->same_price_order != NULL)
					{
						product_node->sell_order_list = sell_order_node->same_price_order;
						temp_sell_node->same_price_order->next = temp_sell_node->next;
					}
					else
					{
						product_node->sell_order_list = sell_order_node->next;
					}
					free(temp_sell_node);
					return 0;
				}
				else
				{
					SellOrder *free_node = sell_order_node;
					if (sell_order_node->same_price_order != NULL)
					{
						temp_sell_node->next = sell_order_node->same_price_order;
					}
					else
					{
						temp_sell_node->next = sell_order_node->next;
					}
					free(free_node);
					return 0;
				}
			}
			else
			{
				SellOrder *same_node = sell_order_node->same_price_order;
				SellOrder *temp_same = same_node;
				while (same_node != NULL)
				{
					if (same_node->trader_id == trader_id && same_node->order_id == order_id)
					{
						*order_type = SELL;
						strcpy(product_name, product_node->product_name);
						if (temp_same == sell_order_node->same_price_order)
						{
							sell_order_node->same_price_order = same_node->same_price_order;
						}
						else
						{
							temp_same->same_price_order = same_node->same_price_order;
						}
						free(same_node);
						return 0;
					}
					temp_same = same_node;
					same_node = same_node->same_price_order;
				}
			}
			temp_sell_node = sell_order_node;
			sell_order_node = sell_order_node->next;
		}
		product_node = product_node->next;
	}
	return -1;
}

void print_info()
{
	printf("%s\t--ORDERBOOK--\n", LOG_PREFIX);
	ProductBook *product_node = m_product_list;
	while (product_node != NULL)
	{
		int buy_levels = 0;
		int sell_levels = 0;
		//get buy levels
		BuyOrder *buy_order_node = product_node->buy_order_list;
		while (buy_order_node != NULL)
		{
			buy_levels++;
			buy_order_node = buy_order_node->next;
		}
		// get sell levels
		SellOrder *sell_order_node = product_node->sell_order_list;
		while (sell_order_node != NULL)
		{
			sell_levels++;
			sell_order_node = sell_order_node->next;
		}
		printf("%s\tProduct: %s; Buy levels: %d; Sell levels: %d\n", LOG_PREFIX, product_node->product_name, buy_levels, sell_levels);

		buy_order_node = product_node->buy_order_list;
		sell_order_node = product_node->sell_order_list;
		int order_count = 0; //same price orders number
		int total_quality = 0;
		while (buy_order_node != NULL || sell_order_node != NULL)
		{
			order_count = 0;
			total_quality = 0;
			if (buy_order_node != NULL && sell_order_node != NULL)
			{
				if (buy_order_node->product_price >= sell_order_node->product_price)
				{
					order_count = 1;
					total_quality = buy_order_node->product_quality;
					BuyOrder *same_node = buy_order_node->same_price_order;
					while (same_node != NULL)
					{
						order_count++;
						total_quality += same_node->product_quality;
						same_node = same_node->same_price_order;
					}
					if (order_count > 1)
					{
						printf("%s\t\tBUY %d @ $%d (%d orders)\n", LOG_PREFIX, total_quality, buy_order_node->product_price, order_count);
					}
					else
					{
						printf("%s\t\tBUY %d @ $%d (%d order)\n", LOG_PREFIX, total_quality, buy_order_node->product_price, order_count);
					}
					buy_order_node = buy_order_node->next;
				}
				else
				{
					order_count = 1;
					total_quality = sell_order_node->product_quality;
					SellOrder *same_node = sell_order_node->same_price_order;
					while (same_node != NULL)
					{
						order_count++;
						total_quality += same_node->product_quality;
						same_node = same_node->same_price_order;
					}
					if (order_count > 1)
					{
						printf("%s\t\tSELL %d @ $%d (%d orders)\n", LOG_PREFIX, total_quality, sell_order_node->product_price, order_count);
					}
					else
					{
						printf("%s\t\tSELL %d @ $%d (%d order)\n", LOG_PREFIX, total_quality, sell_order_node->product_price, order_count);
					}
					sell_order_node = sell_order_node->next;
				}
			}
			else if (buy_order_node != NULL)
			{
				order_count = 1;
				total_quality = buy_order_node->product_quality;
				BuyOrder *same_node = buy_order_node->same_price_order;
				while (same_node != NULL)
				{
					order_count++;
					total_quality += same_node->product_quality;
					same_node = same_node->same_price_order;
				}
				if (order_count > 1)
				{
					printf("%s\t\tBUY %d @ $%d (%d orders)\n", LOG_PREFIX, total_quality, buy_order_node->product_price, order_count);
				}
				else
				{
					printf("%s\t\tBUY %d @ $%d (%d order)\n", LOG_PREFIX, total_quality, buy_order_node->product_price, order_count);
				}
				buy_order_node = buy_order_node->next;
			}
			else if (sell_order_node != NULL)
			{
				order_count = 1;
				total_quality = sell_order_node->product_quality;
				SellOrder *same_node = sell_order_node->same_price_order;
				while (same_node != NULL)
				{
					order_count++;
					total_quality += same_node->product_quality;
					same_node = same_node->same_price_order;
				}
				if (order_count > 1)
				{
					printf("%s\t\tSELL %d @ $%d (%d orders)\n", LOG_PREFIX, total_quality, sell_order_node->product_price, order_count);
				}
				else
				{
					printf("%s\t\tSELL %d @ $%d (%d order)\n", LOG_PREFIX, total_quality, sell_order_node->product_price, order_count);
				}
				sell_order_node = sell_order_node->next;
			}
		}
		product_node = product_node->next;
	}
	printf("%s\t--POSITIONS--\n", LOG_PREFIX);
	Trader *trader_node = m_trader_list;
	while (trader_node != NULL)
	{
		printf("%s\tTrader %d: ", LOG_PREFIX, trader_node->trader_id);
		TradeInfo *trade_data = trader_node->trade_data;
		int count = 0;
		while (trade_data != NULL)
		{
			if (count == 0)
			{
				printf("%s %d ($%lld)", trade_data->product_name, trade_data->total_quality, trade_data->total_amount);
				count++;
			}
			else
			{
				printf(", %s %d ($%lld)", trade_data->product_name, trade_data->total_quality, trade_data->total_amount);
			}
			trade_data = trade_data->next;
		}
		printf("\n");
		trader_node = trader_node->next;
	}
}

//get trader by pid
Trader *get_trader(int trader_pid)
{
	Trader *trader_node = m_trader_list;
	while (trader_node != NULL)
	{
		if (trader_node->trader_pid == trader_pid)
			return trader_node;
		trader_node = trader_node->next;
	}
	return NULL;
}

//check if the product exists or not
bool has_product(char *product_name)
{
	ProductBook *product_node = m_product_list;
	while (product_node != NULL)
	{
		if (strcmp(product_node->product_name, product_name) == 0)
			return true;
		product_node = product_node->next;
	}
	return false;
}

//send invalid messages
void send_invalid_message(Trader *trader_node)
{
	char *reply_msg = "INVALID;";
	int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
	write(fd, reply_msg, strlen(reply_msg));
	kill(trader_node->trader_pid, SIGUSR1);
}

//signal handeller
void sig_recv(int sigNum, siginfo_t *sigInfo, void *context)
{
	if (sigNum == SIGUSR1)
	{
		Trader *trader_node = m_trader_list;
		while (trader_node != NULL)
		{
			if (trader_node->online)
			{
				int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
				if (fd < 0)
				{
					printf("%s Trader %d disconnected\n", LOG_PREFIX, trader_node->trader_id);
					trader_node->online = false;
					m_trader_count--;
				}
			}
			trader_node = trader_node->next;
		}
		//
		trader_node = get_trader(sigInfo->si_pid);
		if (!trader_node)
			return;
		int recv_len = 0;
		char buf[BUF_SIZE] = {0};

		int len = read(trader_node->fifo_control.recv_fd, buf, 1024);
		//keep reading until len >0, not miss any message
		while (len > 0)
		{
			recv_len += len;
			len = read(trader_node->fifo_control.recv_fd, buf + recv_len, 1024);
		}
		if (recv_len > 0)
		{

			int msg_count = 0; //how many lines
			char msg_lines[1024][LINE_SIZE] = {{0}};
			char *msg_line = strtok(buf, ";");
			while (msg_line)
			{
				strcpy(msg_lines[msg_count], msg_line);
				msg_count++;
				msg_line = strtok(NULL, ";");
			}

			for (int i = 0; i < msg_count; i++)
			{
				printf("%s [T%d] Parsing command: <%s>\n", LOG_PREFIX, trader_node->trader_id, msg_lines[i]);

				int word_count = 0;
				char words[5][NAME_SIZE] = {{0}};
				char *word = strtok(msg_lines[i], " ");
				while (word)
				{
					strcpy(words[word_count], word);
					word_count++;
					word = strtok(NULL, " ");
				}

				if (strcmp(words[0], "SELL") == 0)
				{
					if (word_count != 5 || !has_product(words[2]) || atoi(words[3]) <= 0 || atoi(words[4]) <= 0 || atoi(words[3]) > 999999 || atoi(words[4]) > 999999 || atoi(words[1]) != trader_node->current_order_id + 1)
					{
						send_invalid_message(trader_node);
						continue;
					}
					trader_node->current_order_id++;

					char reply_msg[LINE_SIZE] = {0};
					sprintf(reply_msg, "ACCEPTED %s;", words[1]);
					int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
					write(fd, reply_msg, strlen(reply_msg));
					kill(trader_node->trader_pid, SIGUSR1);
					//generate a sell order
					product_sell_order(trader_node->trader_id, atoi(words[1]), words[2], atoi(words[3]), atoi(words[4]));
					print_info();
				}
				else if (strcmp(words[0], "BUY") == 0)
				{
					if (word_count != 5 || !has_product(words[2]) || atoi(words[3]) <= 0 || atoi(words[4]) <= 0 || atoi(words[3]) > 999999 || atoi(words[4]) > 999999 || atoi(words[1]) != trader_node->current_order_id + 1)
					{
						send_invalid_message(trader_node);
						continue;
					}
					trader_node->current_order_id++;

					char reply_msg[LINE_SIZE] = {0};
					sprintf(reply_msg, "ACCEPTED %s;", words[1]);
					int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
					write(fd, reply_msg, strlen(reply_msg));
					kill(trader_node->trader_pid, SIGUSR1);
					product_buy_order(trader_node->trader_id, atoi(words[1]), words[2], atoi(words[3]), atoi(words[4]));
					print_info();
				}
				else if (strcmp(words[0], "AMEND") == 0)
				{
					if (word_count != 4 || atoi(words[1]) > trader_node->current_order_id || atoi(words[2]) < 0 || atoi(words[3]) < 0 || atoi(words[2]) > 999999 || atoi(words[3]) > 999999)
					{
						send_invalid_message(trader_node);
						continue;
					}

					int ret = amend_product_order(trader_node->trader_id, atoi(words[1]), atoi(words[2]), atoi(words[3]));
					if (ret == 0)
					{
						char reply_msg[LINE_SIZE] = {0};
						sprintf(reply_msg, "AMENDED %s;", words[1]);
						int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
						write(fd, reply_msg, strlen(reply_msg));
						kill(trader_node->trader_pid, SIGUSR1);

						print_info();
					}
					else
					{
						send_invalid_message(trader_node);
					}
				}
				else if (strcmp(words[0], "CANCEL") == 0)
				{
					if (word_count == 2 && atoi(words[1]) <= trader_node->current_order_id)
					{
						int order_type;
						char product_name[NAME_SIZE] = {0};
						int ret = cancel_product_order(trader_node->trader_id, atoi(words[1]), product_name, &order_type);
						//successfully cancelled
						if (ret == 0)
						{
							char reply_msg[LINE_SIZE] = {0};
							sprintf(reply_msg, "CANCELLED %s;", words[1]);
							int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
							write(fd, reply_msg, strlen(reply_msg));
							kill(trader_node->trader_pid, SIGUSR1);
							// order_type from the cancel function
							if (order_type == BUY)
								notify_new_order(trader_node->trader_id, "BUY", product_name, 0, 0);
							else
								notify_new_order(trader_node->trader_id, "SELL", product_name, 0, 0);
							print_info();
						}
						else
						{
							send_invalid_message(trader_node);
						}
					}
					else
					{
						send_invalid_message(trader_node);
					}
				}
				//none of the above four cases
				else
				{
					send_invalid_message(trader_node);
				}
			}
			//reset the buffer
			memset(buf, 0, BUF_SIZE);
			//in case current trader send another message
			len = read(trader_node->fifo_control.recv_fd, buf, BUF_SIZE);
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("Not enough arguments\n");
		return 1;
	}

	struct sigaction sigact;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_SIGINFO;
	sigact.sa_handler = (__sighandler_t)sig_recv;
	if (sigaction(SIGUSR1, &sigact, NULL) == -1)
	{
		printf("signal init error\n");
		return -1;
	}

	int product_count = get_product_from_file(argv[1]);
	if (product_count == -1)
		return -1;
	printf("%s Starting\n", LOG_PREFIX);

	int flag = 0;
	printf("%s Trading %d products: ", LOG_PREFIX, product_count);
	ProductBook *product_node = m_product_list;
	while (product_node != NULL)
	{
		if (flag == 0)
		{
			printf("%s", product_node->product_name);
			flag++;
		}
		else
		{
			printf(" %s", product_node->product_name);
		}
		product_node = product_node->next;
	}
	printf("\n");

	Trader *trader_node;
	Trader *temp_trader_node;
	for (int i = 2; i < argc; i++)
	{
		m_trader_count++;
		trader_node = (Trader *)malloc(sizeof(Trader));
		trader_node->trader_id = i - 2;
		trader_node->current_order_id = -1;
		trader_node->trade_data = NULL;
		trader_node->next = NULL;

		product_node = m_product_list;
		TradeInfo *trade_node;
		TradeInfo *temp_trade_node;
		while (product_node != NULL)
		{
			trade_node = (TradeInfo *)malloc(sizeof(TradeInfo));
			strcpy(trade_node->product_name, product_node->product_name);
			trade_node->total_quality = 0;
			trade_node->total_amount = 0;
			trade_node->next = NULL;
			if (trader_node->trade_data == NULL)
			{
				trader_node->trade_data = trade_node;
				temp_trade_node = trade_node;
			}
			else
			{
				temp_trade_node->next = trade_node;
				temp_trade_node = trade_node;
			}
			product_node = product_node->next;
		}
		//reclear the send/rece at first  
		memset(trader_node->fifo_control.send_fifo_name, 0, LINE_SIZE);
		memset(trader_node->fifo_control.recv_fifo_name, 0, LINE_SIZE);
		sprintf(trader_node->fifo_control.send_fifo_name, FIFO_EXCHANGE, trader_node->trader_id);
		sprintf(trader_node->fifo_control.recv_fifo_name, FIFO_TRADER, trader_node->trader_id);
		int ret = mkfifo(trader_node->fifo_control.send_fifo_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (ret != 0)
		{
			printf("fifo create error\n");
			return -1;
		}
		printf("%s Created FIFO %s\n", LOG_PREFIX, trader_node->fifo_control.send_fifo_name);
		ret = mkfifo(trader_node->fifo_control.recv_fifo_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (ret != 0)
		{
			printf("fifo create error\n");
			return -1;
		}
		printf("%s Created FIFO %s\n", LOG_PREFIX, trader_node->fifo_control.recv_fifo_name);
		//create child process (fork())
		printf("%s Starting trader %d (%s)\n", LOG_PREFIX, trader_node->trader_id, argv[i]);
		int trader_pid = fork();
		if (trader_pid == 0)
		{
			char arg[32] = {0};
			sprintf(arg, "%d", trader_node->trader_id);
			execl(argv[i], argv[i], arg, NULL);
			break;
		}
		else
		{
			trader_node->trader_pid = trader_pid;
			trader_node->online = true;
			trader_node->fifo_control.send_fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY);
			printf("%s Connected to %s\n", LOG_PREFIX, trader_node->fifo_control.send_fifo_name);
			trader_node->fifo_control.recv_fd = open(trader_node->fifo_control.recv_fifo_name, O_RDONLY | O_NONBLOCK);
			printf("%s Connected to %s\n", LOG_PREFIX, trader_node->fifo_control.recv_fifo_name);
			if (m_trader_list == NULL)
			{
				m_trader_list = trader_node;
				temp_trader_node = trader_node;
			}
			else
			{
				temp_trader_node->next = trader_node;
				temp_trader_node = trader_node;
			}
		}
	}

	notify_all_trader();
	//check how may trader online to determine close the market or not(trade complete)
	while (m_trader_count > 0)
	{
		sleep(1);
		trader_node = m_trader_list;
		while (trader_node != NULL)
		{
			if (trader_node->online)
			{
				int fd = open(trader_node->fifo_control.send_fifo_name, O_WRONLY | O_NONBLOCK);
				if (fd < 0)
				{
					printf("%s Trader %d disconnected\n", LOG_PREFIX, trader_node->trader_id);
					trader_node->online = false;
					m_trader_count--;
				}
			}
			trader_node = trader_node->next;
		}
	}

	printf("%s Trading completed\n", LOG_PREFIX);
	//
	printf("%s Exchange fees collected: $%lld\n", LOG_PREFIX, total_free_amount);

	clean();

	return 0;
}
