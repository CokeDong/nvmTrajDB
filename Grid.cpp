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

//���Թ���û����
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
		if (i & 1 == 1) //����
		{
			result += (x >> (xbit - 1) & 1) * (1 << (i - 1));
			xbit = xbit + 1;
		}
		else //ż��
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
	//ò��ֻ��Ҫ��һ��ά�Ⱦ����ˣ���Ϊ�涨���˱�����2*2,4*4������
	int divideNumOnX = (int)((mbb.xmax - mbb.xmin) / val_cell_size) + 1; //����Ҫ�ö��ٸ�cell
	int divideNumOnY = (int)((mbb.ymax - mbb.ymin) / val_cell_size) + 1;
	int maxValue = max(divideNumOnX, divideNumOnY);
	//�ҵ���ѵĳ���
	cellNum_axis = maxValue >> (int(log2(maxValue))) << (int(log2(maxValue)) + 1);
	cellnum = cellNum_axis * cellNum_axis;
	cellPtr = new Cell[cellnum];
	//����������������Ҫ����xmax��ymin������չrange

	//ע��cell����Ǵ�(xmin,ymax)��ʼ�ģ�������(xmin,ymin)
	//Z���α���
	for (int i = 0; i <= cellNum_axis - 1; i++) {
		for (int j = 0; j <= cellNum_axis - 1; j++) {
			int cell_idx = getIdxFromXY(j, i);
			cellPtr[cell_idx].initial(i, j, MBB(range.xmin + cell_size*j, range.ymax - cell_size*(i+1), range.xmin + cell_size*(j + 1), range.ymax - cell_size*(i)));
		}
	}
}

//�ѹ켣t������ӹ켣����ӵ�cell����
int Grid::addTrajectoryIntoCell(Trajectory &t)
{
	if (t.length == 0)
		return 1;//�չ켣
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

//ȷ������
int Grid::WhichCellPointIn(SamplePoint p)
{
	//ע��cell����Ǵ�(xmin,ymax)��ʼ�ģ�������(xmin,ymin)
	int row = (int)((range.ymax - p.lat) / cell_size); //��0��ʼ
	int col = (int)((p.lon - range.xmin) / cell_size); //��0��ʼ
	return getIdxFromXY(col, row);
}

int Grid::addDatasetToGrid(Trajectory * db, int traNum)
{
	//ע�⣬�켣��Ŵ�1��ʼ
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
	//ת��Ϊcell�����洢
	//�˴������洢��ָͬһcell�ڵĲ�����洢��һ��������rangeQuery����������similarity query
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
	//�����ɺõ�allpoints�ŵ�GPU��
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
////��Ҫ��д����Ϊ�������ı�
//int Grid::rangeQuery(MBB & bound, CPURangeQueryResult * ResultTable, int* resultSetSize)
//{
//	//�ⲿ��Ҫ��ֲ��gpu�ϣ������õײ㺯��д
//	//Ϊ�˿ɱȽϣ�����������ڽ���Ҫ��ѹ켣����������ˣ�result����֯����QueryResult������
//	//�ж�range�Ƿ񳬳���ͼ
//	ResultTable = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
//	ResultTable->traid = -1; //table��ͷtraidΪ-1 flag
//	ResultTable->next = NULL;
//	CPURangeQueryResult* newResult,* nowResult;
//	nowResult = ResultTable;
//	if (this->range.intersect(bound) != 2)
//		return 1;
//	else
//	{
//		int g1, g2, g3, g4; //box�Ķ�����������
//		int a, b, c, d;//box�Ķ������ڸ��Ӻ�
//		int *candidatesCellID=NULL,*resultsCellID=NULL,*directResultsCellID=NULL;//��ѡ���ӣ�Ĭ��Ϊ��
//		int m, n;//mΪgrid������nΪ����
//		int candidateSize = 0;//candidate����
//		int resultSize,DirectresultSize = 0;//�������
//		int counter = 0;//������
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
//		//��bounding box���߾�����cell����candidates
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
//		//һЩֱ����result
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
//		//������candidateCell��⣬�ɲ���
//		counter = 0;
//		for (int i = 0; i <= candidateSize - 1; i++) {
//			Cell &ce = this->cellPtr[candidatesCellID[i]];
//			for (int j = 0; j <= ce.subTraNum - 1; j++) {
//				int traid = ce.subTraTable[j].traID;
//				int startIdx = ce.subTraTable[j].startpID;
//				int endIdx = ce.subTraTable[j].endpID;
//				for (int k = startIdx; k <= endIdx; k++) {
//					if (bound.pInBox(tradb[traid].points[k].lon, tradb[traid].points[k].lat))//�õ���bound��
//					{
//						newResult = (CPURangeQueryResult*)malloc(sizeof(CPURangeQueryResult));
//						if (newResult == NULL)
//							return 2; //�����ڴ�ʧ��
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
//		//ֱ����Ϊresult��cell�ӽ�resulttable
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
//		//������
//	}
//	return 0;
//}



Grid::~Grid()
{
}
