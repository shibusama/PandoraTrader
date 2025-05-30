#pragma once
#include "cwBasicKindleStrategy.h"
#include "myStructs.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

enum class CloseState {
	Waiting,
	OrderSent,
	PendingCancel,
	Closed,
	Failed
};

struct CloserInstrumentState {
	cwPositionPtr position;
	CloseState state = CloseState::Waiting;
	int retryCount = 0;

	bool isDone() const { return state == CloseState::Closed || state == CloseState::Failed; }
};

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

	//void TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);
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

	virtual void OnStrategyTimer(int iTimerId, const char* szInstrumentID);

	void Run();

private:
	std::map<std::string, futInfMng> tarFutInfo; // 策略上下文
	barInfo comBarInfo;                          // barINfo
	std::map<std::string, int> countLimitCur;    // 合约对应交易数量

	std::map<std::string, cwPositionPtr> CurrentPosMap; //定义map，用于保存挂单信息 
	std::map<cwActiveOrderKey, cwOrderPtr> WaitOrderList; //挂单列表
	std::map<std::string, CloserInstrumentState> instrumentStates; // 订单状态

	void UpdatePositions();
	bool IsAllDone() const;
	void HandleInstrument(const std::string& id, CloserInstrumentState& state);
	bool TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);
	cwOrderPtr SafeLimitOrder(cwMarketDataPtr md, int volume, double slipTick, double tickSize);
};