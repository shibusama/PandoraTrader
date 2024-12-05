#pragma once
#include <string>
#include "sqlite3.h"
#include <map>
#include <vector>


namespace MyTrade {

    // 持仓状况结构体
    struct catePortInf {
        std::string direction;
        int volume;
        double costPrice;
        double openCost;
        int amount;
    };

    // 合约乘数与一跳价位结构体
    struct marginovk {
        int multiple;
        double ticksize;
    };

    // 主力合约键结构体
    struct mainCtrKeys {
        std::string date;
        std::string contract;
    };

    // 主力合约值结构体
    struct mainCtrValues {
        std::string code;
        double factor;
        double accfactor;
    };

    // 分钟数据信息结构体
    struct barFuture {
        std::string code;
        std::string tradingday;
        std::string timestamp;
        int volume;
        double price;
    };

    // 以下结构体如果有需要可以取消注释并使用
    // 仓位管理结构体（原posMng，这里先注释掉，你可按需启用）
    // struct posMng {
    //     std::string code;
    //     std::string direction;
    //     int volume;
    //     double costPrice;
    //     double openCost;
    //     int amount;
    // };

    // 开仓管理结构体
    struct orcMng {
        std::string cdate;
        std::string ctime;
        std::string dire;
        int htime;
        double profit;
        std::string method;
    };

    // 策略参数信息结构体
    struct paraMng {
        std::string Fac;
        std::string ver;
        std::string typ;
        int Rs;
        int Rl;
    };

    // 期货合约信息结构体
    struct futInfMng {
        std::string exchagne;
        int multiple;
        double ticksize;
        double marginrate;
    };

    class Class1 {
    public:
        sqlite3* cnn;
        sqlite3* cnnSys;
        const char* dbFilePath;

        std::string getCurrentDateString(); 

        double ArithmeticMean(double arr[], int size); //计算简单算数平均值

        double SampleStd(double arr[], int size); //计算样本标准差

        std::map<mainCtrKeys, mainCtrValues> MainInf;

        // 历史行情数据，键为string类型，值为barFuture结构体的vector（相当于C#中的List）
        std::map<std::string, std::vector<barFuture>> barFlow;

        // 新增行情数据
        std::map<std::string, std::vector<barFuture>> barFlowCur;

        // 因子数据
        std::map<std::string, double> factorDictCur;

        // 目标交易合约
        std::map<std::string, std::string> codeTractCur;

        // 期货合约信息，键为string类型，值为futInfMng结构体
        std::map<std::string, futInfMng> futInfDict;

        // 行情数据，键为string类型，值为double类型的vector
        std::map<std::string, std::vector<double>> queueBar;

        // 收益率数据
        std::map<std::string, std::vector<double>> retBar;

        // 当前持仓情况，键为string类型，值为catePortInf结构体
        std::map<std::string, catePortInf> spePos;

        // 策略参数对应信息
        std::map<std::string, paraMng> verDictCur;

        // 合约对应交易数量
        std::map<std::string, int> countLimitCur;

        std::vector<std::string> tarCateList;

        std::string cursor_str;

        std::string m_strCurrentUpdateTime;
	};


}