#pragma once
#pragma once
#include <string>
#include <map>
#include <memory>
#include <iostream>
//#include "cwContext.h"  // ���趨���� cwPositionPtr��cwOrderPtr��cwActiveOrderKey ��
#include "cwBasicKindleStrategy.h" // ���� GetPositionsAndActiveOrders ����
#include "Utils.hpp"

class cwInstrumentCloser {
public:
    cwInstrumentCloser(cwBasicKindleStrategy* context, const std::string& instrumentID);

    void RunOnce(int hour, int minute, int second);
    bool IsFinished() const;

private:
    void FetchData();                       // ��ȡ���³ֲֺ͹ҵ�
    void CancelPendingOrders();            // �����ҵ�
    void TryClose();                       // ������ֵ�
    bool HasPosition() const;
    bool HasPendingOrder() const;
    void TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);

private:
    cwBasicKindleStrategy* ctx;
    std::string instrumentID;

    cwPositionPtr position;
    std::map<cwActiveOrderKey, cwOrderPtr> waitOrders;

    int lastAttemptTime = 0;
    int retryCount = 0;
    bool finished = false;
};
