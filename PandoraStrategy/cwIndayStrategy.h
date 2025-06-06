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
	///�������
	virtual void PriceUpdate(cwMarketDataPtr pPriceData);
	//������һ����K�ߵ�ʱ�򣬻���øûص�
	virtual void OnBar(cwMarketDataPtr pPriceData, int iTimeScale, cwBasicKindleStrategy::cwKindleSeriesPtr pKindleSeries);
	//Trade SPI
	///�ɽ��ر�
	virtual void OnRtnTrade(cwTradePtr pTrade);
	//�����ر�
	virtual void OnRtnOrder(cwOrderPtr pOrder, cwOrderPtr pOriginOrder = cwOrderPtr());
	//�����ɹ�
	virtual void OnOrderCanceled(cwOrderPtr pOrder);
	//�����Խ��׳�ʼ�����ʱ�����OnReady, �����ڴ˺��������Եĳ�ʼ������
	virtual void OnReady();
	//��ʼ������������
	void UpdateBarData();
	// �Զ�ƽ��ֺ���
	void AutoCloseAllPositionsLoop();

	bool TryAggressiveClose(cwMarketDataPtr md, orderInfo info);
	//��ǰʱ��
	std::string m_strCurrentUpdateTime;
	// bar����
	void UpdateCtx(cwMarketDataPtr pPriceData);
	// ���ֽ��� ����
	void StrategyPosOpen(cwMarketDataPtr, std::unordered_map<std::string, orderInfo>& cwOrderInfo);
	// ƽ�ֽ��� ����
	void StrategyPosClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos, std::unordered_map<std::string, orderInfo>& cwOrderInfo);

	bool IsPendingOrder(std::string instrumentID);

	std::string GetPositionDirection(cwPositionPtr pPos);

	//void cwIndayStrategy::CloseAllPositionWithRetry(const std::string& instrumentID);	

	cwOrderPtr SafeLimitOrder(cwMarketDataPtr md, int volume, double slipTick, double tickSize);

	virtual void OnStrategyTimer(int iTimerId, const char* szInstrumentID);

	void GenCloseOrder(cwMarketDataPtr pPriceData, std::unordered_map<std::string, orderInfo>& cwOrderInfo);

private:
	std::map<std::string, futInfMng> tarFutInfo; // ����������
	barInfo comBarInfo;                          // barINfo
	std::map<std::string, int> countLimitCur;    // ��Լ��Ӧ��������
	std::map<cwActiveOrderKey, cwOrderPtr> strategyWaitOrderList;           // �ҵ��б�ȫ�֣�


	//�������ȫ�ֱ���
	std::unordered_map<std::string, bool> instrumentCloseFlag;      // �Ƿ񴥷�����ƽ��
	std::unordered_map<std::string, int> lastCloseAttemptTime;      // ��Լ->�ϴ���ֳ���ʱ������룩
	std::unordered_map<std::string, int> closeAttemptCount;         // ���ڿ����ع�Ƶ�ʣ�ÿ����Լ��

	//��������ȫ�ֱ���
	std::unordered_map<std::string, orderInfo> cwOrderInfo;
};