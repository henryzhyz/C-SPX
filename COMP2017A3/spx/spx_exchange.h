#ifndef SPX_EXCHANGE_H
#define SPX_EXCHANGE_H

#include "spx_common.h"
#include <stdbool.h>

#define LOG_PREFIX "[SPX]"

#endif

typedef struct FIFOControl
{
	char send_fifo_name[LINE_SIZE];
	char recv_fifo_name[LINE_SIZE];
	int recv_fd;
	int send_fd;
} FIFOControl;

typedef struct SellOrder
{
	int order_id;
	int product_price;
	int product_quality;
	int trader_id;
	struct SellOrder *same_price_order;
	struct SellOrder *next;
} SellOrder;

typedef struct BuyOrder
{
	int order_id;
	int product_price;
	int product_quality;
	int trader_id;
	struct BuyOrder *same_price_order;
	struct BuyOrder *next;
} BuyOrder;

typedef struct ProductBook
{
	char product_name[NAME_SIZE];
	SellOrder *sell_order_list;
	BuyOrder *buy_order_list;
	struct ProductBook *next;
} ProductBook;

typedef struct TradeInfo
{
	char product_name[NAME_SIZE];
	int total_quality;
	long long total_amount;
	struct TradeInfo *next;
} TradeInfo;

typedef struct Trader
{
	int trader_id;
	int trader_pid;
	bool online;
	FIFOControl fifo_control;
	int current_order_id;
	TradeInfo *trade_data;
	struct Trader *next;
} Trader;
