#pragma once
#include "cwBasicKindleStrategy.h"
#include "myStructs.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

class cwIndayStrategy :
	public cwBasicKindleStrategy
{
public:
	cwIndayStrategy();
	~cwIndayStrategy();

	//MarketData SPI
	///行情更新
	virtual void PriceUpdate(cwMarketDataPtr pPriceData);
	//当生成一根新K线的时候，会调用该回调
	virtual void OnBar(cwMarketDataPtr pPriceData, int iTimeScale, cwBasicKindleStrategy::cwKindleSeriesPtr pKindleSeries);
	//Trade SPI
	///成交回报
	virtual void OnRtnTrade(cwTradePtr pTrade);
	//报单回报
	virtual void OnRtnOrder(cwOrderPtr pOrder, cwOrderPtr pOriginOrder = cwOrderPtr());
	//撤单成功
	virtual void OnOrderCanceled(cwOrderPtr pOrder);
	//当策略交易初始化完成时会调用OnReady, 可以在此函数做策略的初始化操作
	virtual void OnReady();
	//初始化策略上下文
	void UpdateBarData();
	// 自动平昨仓函数
	void AutoCloseAllPositionsLoop();

	bool TryAggressiveClose(cwMarketDataPtr md, orderInfo info);
	//当前时间
	std::string m_strCurrentUpdateTime;
	// bar更新
	void UpdateCtx(cwMarketDataPtr pPriceData);
	// 开仓交易 条件
	void StrategyPosOpen(cwMarketDataPtr, std::unordered_map<std::string, orderInfo>& cwOrderInfo);
	// 平仓交易 条件
	void StrategyPosClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos, std::unordered_map<std::string, orderInfo>& cwOrderInfo);

	bool IsPendingOrder(std::string instrumentID);

	std::string GetPositionDirection(cwPositionPtr pPos);

	//void cwIndayStrategy::CloseAllPositionWithRetry(const std::string& instrumentID);	

	cwOrderPtr SafeLimitOrder(cwMarketDataPtr md, int volume, double slipTick, double tickSize);

	virtual void OnStrategyTimer(int iTimerId, const char* szInstrumentID);

	void GenCloseOrder(cwMarketDataPtr pPriceData, std::unordered_map<std::string, orderInfo>& cwOrderInfo);

private:
	std::map<std::string, futInfMng> tarFutInfo; // 策略上下文
	barInfo comBarInfo;                          // barINfo
	std::map<std::string, int> countLimitCur;    // 合约对应交易数量
	std::map<cwActiveOrderKey, cwOrderPtr> strategyWaitOrderList;           // 挂单列表（全局）


	//清仓所需全局变量
	std::unordered_map<std::string, bool> instrumentCloseFlag;      // 是否触发收盘平仓
	std::unordered_map<std::string, int> lastCloseAttemptTime;      // 合约->上次清仓尝试时间戳（秒）
	std::unordered_map<std::string, int> closeAttemptCount;         // 用于控制重挂频率（每个合约）

	//交易所需全局变量
	std::unordered_map<std::string, orderInfo> cwOrderInfo;
};