#pragma once
#include "cwBasicKindleStrategy.h"
#include "myStructs.h"

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

	void TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);
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

	cwOrderPtr SafeLimitOrder(
		const char* instrumentID,
		int volume,                        // >0买 <0卖
		double rawPrice,                  // 原始价格（策略计算的目标价格）
		double slipTick = 1.0            // 滑价 tick 数，默认滑 1 tick
	);

	virtual void OnStrategyTimer(int iTimerId, const char* szInstrumentID);

private:
	std::map<std::string, futInfMng> tarFutInfo; // 策略上下文
	barInfo comBarInfo;                          // barINfo
	std::map<std::string, int> countLimitCur;    // 合约对应交易数量
	std::map<cwActiveOrderKey, cwOrderPtr> strategyWaitOrderList;           // 挂单列表（全局）
};