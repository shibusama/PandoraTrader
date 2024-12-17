#include "inDay.h"
#include "SqlliteHelp.h"
using namespace std;
namespace MyTrade {

	// 创建数据库连接
	sqlite3* Class1::cnn = SqlliteHelp::OpenDatabase("D:/sqllite/tmp.db");
	sqlite3* Class1::cnnSys = SqlliteHelp::OpenDatabase("D:/sqllite/tmp.db");

	map<mainCtrKeys, mainCtrValues>* Class1::MainInf = new map<mainCtrKeys, mainCtrValues>;
	map<string, vector<barFuture>>* Class1::barFlow = new map<string, vector<barFuture>>;

	map<string, vector<barFuture>>* Class1::barFlowCur = new map<string, vector<barFuture>>;
	map<string, double>* Class1::factorDictCur = new map<string, double>;
	map<string, string>* Class1::codeTractCur = new map<string, string>;
	map<string, futInfMng>* Class1::futInfDict = new map<string, futInfMng>;

	map<string, vector<double>>* Class1::queueBar = new map<string, vector<double>>;
	map<string, vector<double>>* Class1::retBar = new map<string, vector<double>>;

	map<string, catePortInf>* Class1::spePos = new map<string, catePortInf>;
	map<string, paraMng>* Class1::verDictCur = new map<string, paraMng>;
	map<string, int>* Class1::countLimitCur = new map<string, int>;

	vector<string>* Class1::tarCateList = new vector<string>;

	string cursor_str; // 交易当天日期


}