#include "inDay.h"
#include "SqlliteHelp.h"



using namespace MyTrade;


// 创建数据库连接
sqlite3* cnn = SqlliteHelp::OpenDatabase("D:/sqllite/tmp.db");
sqlite3* cnnSys = SqlliteHelp::OpenDatabase("D:/sqllite/tmp.db");

std::map<mainCtrKeys, mainCtrValues>* MainInf = new std::map<mainCtrKeys, mainCtrValues>;
std::map<std::string, std::vector<barFuture>>* barFlow = new std::map<std::string, std::vector<barFuture>>;

std::map<std::string, std::vector<barFuture>>* barFlowCur = new std::map<std::string, std::vector<barFuture>>;
std::map<std::string, double>* factorDictCur = new std::map<std::string, double>;
std::map<std::string, std::string>* codeTractCur = new std::map<std::string, std::string>;
std::map<std::string, futInfMng>* futInfDict = new std::map<std::string, futInfMng>;

std::map<std::string, std::vector<double>>* queueBar = new std::map<std::string, std::vector<double>>;
std::map<std::string, std::vector<double>>* retBar = new std::map<std::string, std::vector<double>>;

std::map<std::string, catePortInf>* spePos = new std::map<std::string, catePortInf>;
std::map<std::string, paraMng>* verDictCur = new std::map<std::string, paraMng>;
std::map<std::string, int>* countLimitCur = new std::map<std::string, int>;

std::vector<std::string>* tarCateList =new std::vector<std::string>;

std::string cursor_str; // 交易当天日期


