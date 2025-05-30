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

	//void TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);
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

	virtual void OnStrategyTimer(int iTimerId, const char* szInstrumentID);

	void Run();

private:
	std::map<std::string, futInfMng> tarFutInfo; // ����������
	barInfo comBarInfo;                          // barINfo
	std::map<std::string, int> countLimitCur;    // ��Լ��Ӧ��������

	std::map<std::string, cwPositionPtr> CurrentPosMap; //����map�����ڱ���ҵ���Ϣ 
	std::map<cwActiveOrderKey, cwOrderPtr> WaitOrderList; //�ҵ��б�
	std::map<std::string, CloserInstrumentState> instrumentStates; // ����״̬

	void UpdatePositions();
	bool IsAllDone() const;
	void HandleInstrument(const std::string& id, CloserInstrumentState& state);
	bool TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);
	cwOrderPtr SafeLimitOrder(cwMarketDataPtr md, int volume, double slipTick, double tickSize);
};