#include "Grid.h"
#include "WinTimer.h"

extern Trajectory* tradb;
MyTimer timer;

Grid::Grid()
{
	range = MBB(0, 0, 0, 0);
	cellnum = 0;
	cell_size = 0;
	cellNum_axis = 0;
	cellPtr = NULL;

}

//测试过，没问题
int Grid::getIdxFromXY(int x, int y)
{
	int lenx, leny;
	if (x == 0)
		lenx = 1;
	else
	{
		lenx = int(log2(x)) + 1;
	}
	if (y == 0)
		leny = 1;
	else
		leny = int(log2(y)) + 1;
	int result = 0;
	int xbit = 1, ybit = 1;
	for (int i = 1; i <= (lenx + leny); i++) {
		if (i & 1 == 1) //奇数
		{
			result += (x >> (xbit - 1) & 1) * (1 << (i - 1));
			xbit = xbit + 1;
		}
		else //偶数
		{
			result += (y >> (ybit - 1) & 1) * (1 << (i - 1));
			ybit = ybit + 1;
		}
	}
	return result;
}

Grid::Grid(const MBB& mbb,float val_cell_size)
{
	range = mbb;
	cell_size = val_cell_size;
	//貌似只需要用一个维度就行了，因为规定好了必须是2*2,4*4，……
	int divideNumOnX = (int)((mbb.xmax - mbb.xmin) / val_cell_size) + 1; //最少要用多少个cell
	int divideNumOnY = (int)((mbb.ymax - mbb.ymin) / val_cell_size) + 1;
	int maxValue = max(divideNumOnX, divideNumOnY);
	//找到最佳的长宽
	cellNum_axis = maxValue >> (int(log2(maxValue))) << (int(log2(maxValue)) + 1);
	cellnum = cellNum_axis * cellNum_axis;
	cellPtr = new Cell[cellnum];
	//由于满足正方形需要，向xmax、ymin方向扩展range

	//注意cell编号是从(xmin,ymax)开始的，而不是(xmin,ymin)
	//Z字形编码
	for (int i = 0; i <= cellNum_axis - 1; i++) {
		for (int j = 0; j <= cellNum_axis - 1; j++) {
			int cell_idx = getIdxFromXY(j, i);
			cellPtr[cell_idx].initial(i, j, MBB(range.xmin + cell_size*j, range.ymax - cell_size*(i+1), range.xmin + cell_size*(j + 1), range.ymax - cell_size*(i)));
		}
	}
}

//把轨迹t打碎成子轨迹，添加到cell里面
int Grid::addTrajectoryIntoCell(Trajectory &t)
{
	if (t.length == 0)
		return 1;//空轨迹
	SamplePoint p = t.points[0];
	int lastCellNo = WhichCellPointIn(p); 
	int lastCellStartIdx = 0;
	int nowCellNo;
	for (int i = 1; i <= t.length - 1; i++) {
		p = t.points[i];
		nowCellNo = WhichCellPointIn(p);
		if (i == t.length - 1)
		{
			if (lastCellNo == nowCellNo)
			{
				cellPtr[nowCellNo].addSubTra(t.tid, lastCellStartIdx, i, i - lastCellStartIdx + 1);
				return 0;
			}
			else
			{
				cellPtr[lastCellNo].addSubTra(t.tid, lastCellStartIdx, i - 1, i - 1 - lastCellStartIdx + 1);
				cellPtr[nowCellNo].addSubTra(t.tid, i, i, 1);
				return 0;
			}
		}
		else
		{
			if (lastCellNo == nowCellNo)
				continue;
			else
			{
				cellPtr[lastCellNo].addSubTra(t.tid, lastCellStartIdx, i - 1, i - 1 - lastCellStartIdx + 1);
				lastCellNo = nowCellNo;
				lastCellStartIdx = i;
			}
		}
	}
	return 0;
}

//确认无误
int Grid::WhichCellPointIn(SamplePoint p)
{
	//注意cell编号是从(xmin,ymax)开始的，而不是(xmin,ymin)
	int row = (int)((range.ymax - p.lat) / cell_size); //从0开始
	int col = (int)((p.lon - range.xmin) / cell_size); //从0开始
	return getIdxFromXY(col, row);
}

int Grid::addDatasetToGrid(Trajectory * db, int traNum)
{
	//注意，轨迹编号从1开始
	int pointCount = 0;
	for (int i = 1; i <= traNum; i++) {
		addTrajectoryIntoCell(db[i]);
	}
	for (int i = 0; i <= cellnum - 1; i++) {
		cellPtr[i].buildSubTraTable();
		pointCount += cellPtr[i].totalPointNum;
	}
	this->totalPointNum = pointCount;

#ifdef _CELL_BASED_STORAGE
	//转化为cell连续存储
	//此处连续存储是指同一cell内的采样点存储在一起，有利于rangeQuery，但不利于similarity query
	this->allPoints = (Point*)malloc(sizeof(Point)*(this->totalPointNum));
	pointCount = 0;
	for (int i = 0; i <= cellnum - 1; i++) {
		cellPtr[i].pointRangeStart = pointCount;
		for (int j = 0; j <= cellPtr[i].subTraNum - 1; j++) {
			for (int k = cellPtr[i].subTraTable[j].startpID; k <= cellPtr[i].subTraTable[j].endpID; k++) {
				allPoints[pointCount].tID = cellPtr[i].subTraTable[j].traID;
				allPoints[pointCount].x = tradb[allPoints[pointCount].tID].points[k].lon;
				allPoints[pointCount].y = tradb[allPoints[pointCount].tID].points[k].lat;
				allPoints[pointCount].time = tradb[allPoints[pointCount].tID].points[k].time;
				pointCount++;
			}
		}
		cellPtr[i].pointRangeEnd = pointCount - 1;
		if (cellPtr[i].pointRangeEnd - cellPtr[i].pointRangeStart + 1 != cellPtr[i].totalPointNum)
			cerr << "Grid.cpp: something wrong in total point statistic" << endl;
	}
	//把生成好的allpoints放到GPU内
	putCellDataSetIntoGPU(this->allPoints, this->allPointsPtrGPU, this->totalPointNum);

#endif // _CELL_BASED_STORAGE

	return 0;
}

int Grid::writeCellsToFile(int * cellNo,int cellNum, string file)
// under editing....
{
	fout.open(file, ios_base::out);
	for (int i = 0; i <= cellNum - 1; i++) {
		int outCellIdx = cellNo[i];
		cout << outCellIdx << ": " << "[" << cellPtr[outCellIdx].mbb.xmin << "," <<cellPtr[outCellIdx].mbb.xmax << "," << cellPtr[outCellIdx].mbb.ymin << "," << cellPtr[outCellIdx].mbb.ymax << "]" << endl;
		for (int j = 0; j <= cellPtr[outCellIdx].subTraNum - 1; j++) {
			int tid = cellPtr[outCellIdx].subTraTable[j].traID;
			int startpid = cellPtr[outCellIdx].subTraTable[j].startpID;
			int endpid = cellPtr[outCellIdx].subTraTable[j].endpID;
			for (int k = startpid; k <= endpid; k++) {
				cout << tradb[tid].points[k].lat << "," << tradb[tid].points[k].lon << ";";
			}
			cout << endl;
		}
	}
	return 0;
}

////int Grid::rangeQuery(MBB & bound, int * ResultTraID, SamplePoint ** ResultTable,int* resultSetSize,int* resultTraLength)
////需要重写，因为编码规则改变
//int Grid::rangeQuery(MBB & bound, CPURangeQueryResult * ResultTable, int* resultSetSize)
//{
//	//这部分要移植到gpu上，尽量用底层函数写
//	//为了可比较，在这个函数内仅仅要求把轨迹查出来就行了，result的组织交由QueryResult类来做
//	//判断range是否超出地图
//	ResultTable = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
//	ResultTable->traid = -1; //table开头traid为-1 flag
//	ResultTable->next = NULL;
//	CPURangeQueryResult* newResult,* nowResult;
//	nowResult = ResultTable;
//	if (this->range.intersect(bound) != 2)
//		return 1;
//	else
//	{
//		int g1, g2, g3, g4; //box的顶点所在行列
//		int a, b, c, d;//box的顶点所在格子号
//		int *candidatesCellID=NULL,*resultsCellID=NULL,*directResultsCellID=NULL;//候选格子，默认为空
//		int m, n;//m为grid列数，n为行数
//		int candidateSize = 0;//candidate个数
//		int resultSize,DirectresultSize = 0;//结果个数
//		int counter = 0;//计数器
//		m = this->cell_num_x;
//		n = this->cell_num_y;
//		g1 = (int)((bound.xmin - range.xmin) / cell_size);
//		g2 = (int)((bound.xmax - range.xmin) / cell_size);
//		g3 = (int)((range.ymax - bound.ymax) / cell_size);
//		g4 = (int)((range.ymax - bound.ymin) / cell_size);
//		//for test
//		//g1 = test[0];
//		//g2 = test[1];
//		//g3 = test[2];
//		//g4 = test[3];
//		//m = 10;
//		//n = 10;
//
//		a = g1 + g3*m;
//		b = g2 + g3*m;
//		c = g1 + g4*m;
//		d = g2 + g4*m;
//
//		if (a == b){
//			candidateSize = (c - a) / m + 1;
//		}
//		else {
//			if (a == c)
//				candidateSize = (b - a) + 1;
//			else
//				candidateSize = ((c - a) / m + 1) * 2 + (b - a + 1) * 2 - 4;
//		}
//		//把bounding box的线经过的cell加入candidates
//		candidatesCellID = (int*)malloc(sizeof(int)*candidateSize);
//		counter = 0;
//		for (int i = a; i <= b; i++) {
//			candidatesCellID[counter] = i;
//			counter++;
//		}
//		for (int i = c; i <= d; i++) {
//			candidatesCellID[counter] = i;
//			counter++;
//		}
//		if (g4 - g3 >= 2) {
//			for (int i = a + m; i <= a + (g4 - g3- 1)*m; i = i + m) {
//				candidatesCellID[counter] = i;
//				counter++;
//			}
//			for (int i = b + m; i <= b + (g4 - g3- 1)*m; i = i + m) {
//				candidatesCellID[counter] = i;
//				counter++;
//			}
//		}
//		if (counter != candidateSize)
//			cerr << "size error in range query candidates cell" << endl;
//
//		//一些直接是result
//		DirectresultSize = (b - a - 1)*(g4 - g3 - 1);
//		counter = 0;
//		directResultsCellID = (int*)malloc(DirectresultSize * sizeof(int));
//		if (b >= a + 2 && c >= a + 2 * m) {
//			for (int i = a + 1; i <= b - 1; i++) {
//				for (int j = 1; j <= g4 - g3 - 1; j++) {
//					directResultsCellID[counter] = i + j*m;
//					counter++;
//				}
//			}
//		}
//		if (counter != DirectresultSize)
//			cerr << "size error in range query directresult cell" <<counter<<","<<candidateSize<< endl;
//		timer.start();
//		//对所有candidateCell检测，可并行
//		counter = 0;
//		for (int i = 0; i <= candidateSize - 1; i++) {
//			Cell &ce = this->cellPtr[candidatesCellID[i]];
//			for (int j = 0; j <= ce.subTraNum - 1; j++) {
//				int traid = ce.subTraTable[j].traID;
//				int startIdx = ce.subTraTable[j].startpID;
//				int endIdx = ce.subTraTable[j].endpID;
//				for (int k = startIdx; k <= endIdx; k++) {
//					if (bound.pInBox(tradb[traid].points[k].lon, tradb[traid].points[k].lat))//该点在bound内
//					{
//						newResult = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
//						if (newResult == NULL)
//							return 2; //分配内存失败
//						newResult->traid = tradb[traid].points[k].tid;
//						newResult->x = tradb[traid].points[k].lon;
//						newResult->y = tradb[traid].points[k].lat;
//						newResult->next = NULL;
//						nowResult->next = newResult;
//						nowResult = newResult;
//						counter++;
//					}
//				}
//			}
//		}
//		timer.stop();
//		cout << "CPU time:" << timer.elapse() << "ms" <<endl;
//		//直接作为result的cell加进resulttable
//		for (int i = 0; i <= DirectresultSize - 1; i++) {
//			Cell &ce = this->cellPtr[directResultsCellID[i]];
//			for (int j = 0; j <= ce.subTraNum - 1; j++) {
//				int traid = ce.subTraTable[j].traID;
//				int startIdx = ce.subTraTable[j].startpID;
//				int endIdx = ce.subTraTable[j].endpID;
//				for (int k = startIdx; k <= endIdx; k++) {
//					newResult = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
//					newResult->traid = tradb[traid].points[k].tid;
//					newResult->x = tradb[traid].points[k].lon;
//					newResult->y = tradb[traid].points[k].lat;
//					newResult->next = NULL;
//					nowResult->next = newResult;
//					nowResult = newResult;
//					counter++;
//				}
//			}
//		}
//		(*resultSetSize) = counter;
//		//输出结果
//	}
//	return 0;
//}



Grid::~Grid()
{
}
