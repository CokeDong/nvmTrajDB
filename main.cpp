// Data_Structure_Test.cpp : 定义控制台应用程序的入口点。
//

#include <iostream>
#include <fstream>
#include <map>
#include "CalcInAxis.h"
#include "PreProcess.h"
#include "Trajectory.h"
#include <vector>
#include "Grid.h"

using namespace std;

map<string, tidLinkTable*> vidTotid;
map<string, tidLinkTable*>::iterator iter;

//global
Trajectory* tradb;
string baseDate = "2014-07-01";


int main()
{
	
	int WriteTrajectoryToFile(string outFileName, int numTra);
	cout << "Hello world!" << endl;
	cout << sizeof(Cell) << endl;
	//ifstream fin;
	//float lon1, lat1, lon2, lat2;
	//lat1 = +34.15265;
	//lon1 = +113.10221;
	//lat2 = +35.15221;
	//lon2 = +113.10222;
	//cout << calculateDistance(lat1, lon1, lat2, lon2) << endl;
	PreProcess pp("SH_1.txt", "dataout.txt");
	cout << WriteTrajectoryToFile("dataOut.txt", pp.maxTid) << endl;
	cout << "read trajectory success!" << endl << "Start building cell index" << endl;
	Grid* g = new Grid(MBB(pp.xmin, pp.ymin, pp.xmax, pp.ymax), 0.003);
	g->addDatasetToGrid(tradb, pp.maxTid);
	int count = 0;
	for (int i = 0; i <= g->cellnum - 1; i++) {
		if (g->cellPtr[i].subTraNum == 0)
			count++;
	}
	cout << "zero num:" << count << "total" << g->cellnum << endl;
	//int temp[7] = { 553,554,555,556,557,558,559 };
	//int sizetemp = 7;
	//g->writeCellsToFile(temp, sizetemp, "111.txt");
	//CPURangeQueryResult* resultTable=NULL;
	//int RangeQueryResultSize = 0;
	//g->rangeQuery(MBB(121.4, 31.15, 121.6, 31.25), resultTable, &RangeQueryResultSize);

	ofstream ftest;
	ftest.open("ftest.txt", ios_base::out);
	ftest << g->totalPointNum << endl;
	for (int i = 0; i <= g->cellnum - 1; i++) {
		ftest << g->cellPtr[i].totalPointNum << ",";
	}
	ftest << endl;


	cout << "build cell index success!" << endl;
	getchar();
	getchar();
	getchar();

	return 0;
}

int WriteTrajectoryToFile(string outFileName,int numTra) {
	ofstream fout;
	fout.open(outFileName, ios_base::out);
	for (int i = 1; i <= numTra; i++) {
		fout << i << ": ";
		for (int j = 0; j <= tradb[i].length - 1; j++) {
			fout << tradb[i].points[j].lon << "," << tradb[i].points[j].lat << ";";
		}
		fout << endl;
	}
	fout.close();
	return 1;
}
