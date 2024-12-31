#pragma once
#include <fstream>
#include "cwBasicKindleStrategy.h"
#include <iostream>
using namespace std;
class cwEmptyStrategy :
	public cwBasicKindleStrategy
{
public:
	cwEmptyStrategy();
	~cwEmptyStrategy();

	string  GetStrategyName();

	//MarketData SPI
	///行情更新
	virtual void PriceUpdate(cwMarketDataPtr pPriceData);

	//Trade SPI
	///成交回报
	virtual void OnRtnTrade(cwTradePtr pTrade) {};
	///报单回报
	virtual void OnRtnOrder(cwOrderPtr pOrder, cwOrderPtr pOriginOrder = cwOrderPtr()) {};
	///撤单成功
	virtual void OnOrderCanceled(cwOrderPtr pOrder) {};

	virtual void OnReady();

	string	m_strCurrentUpdateTime;	


	void InitialStrategy(const char * pConfigFilePath);

	///strategy parameter
	//策略运行代号
	string m_strStrategyName;		
	//策略是否运行
	bool		m_bStrategyRun;					

	bool		m_bShowPosition;
private:

};

