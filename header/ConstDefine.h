#pragma once
#define MAX_TRAJ_SIZE 100000
#define MAXLENGTH 512
//MAXGAP是最大轨迹内时间间隔，如果超过这个间隔应该被视为两条轨迹
#define MAXGAP 3600

#define EPSILON 10
#define MAXTHREAD 512

#include <stdio.h>
#include <string>
#include <math.h>


typedef struct Point {
	float x;
	float y;
	uint32_t time;
	uint32_t tID;
}Point;

typedef struct SPoint {
	float x;
	float y;
	uint32_t tID;
}SPoint;
