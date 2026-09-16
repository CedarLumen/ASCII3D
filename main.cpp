#include <iostream>
#include <string>
#include <windows.h>
#include <cstdlib>
#include <cmath>
#include <cstring>
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 60
#define MAX_TRIANGLE_QUANTITY 1000
#define EPSILON 1e-6
using namespace std;

char screenBuffer[SCREEN_HEIGHT][SCREEN_WIDTH];
double depthBuffer[SCREEN_HEIGHT][SCREEN_WIDTH];
char curLineChar = '#';
char curSurfaceChar = '*';
template <typename T>
struct Dot3 {T x, y, z;};
int triangleCurPtr = 0;
int triangleEndPtr = -3;
Dot3<double>* triangleBuffer = (Dot3<double>*)malloc(sizeof(Dot3<double>) * MAX_TRIANGLE_QUANTITY * 3);
Dot3<double>* triangleOrthoBuffer = (Dot3<double>*)malloc(sizeof(Dot3<double>) * MAX_TRIANGLE_QUANTITY * 3);



Dot3<double> operator+(const Dot3<double>& a, const Dot3<double>& b) {
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Dot3<double> operator-(const Dot3<double>& a, const Dot3<double>& b) {
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Dot3<double> operator*(const Dot3<double>& a, double b) {
	return {a.x * b, a.y * b, a.z * b};
}
Dot3<double> operator/(const Dot3<double>& a, double b) {
	return {a.x / b, a.y / b, a.z / b};
}
Dot3<double> operator^(const Dot3<double>& a, const Dot3<double>& b) {
	return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
struct Matrix4x4 {
	double m[4][4];
};

Matrix4x4 operator*(const Matrix4x4& a, const Matrix4x4& b) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.m[i][j] = 0;
			for (int k = 0; k < 4; ++k) {
				result.m[i][j] += a.m[i][k] * b.m[k][j];
			}
		}
	}
	return result;
}

Dot3<double> testVertex = { 1.0, 2.0, 1.0 };
Matrix4x4 testCameraPos = { {
	{ 1.0, 0.0, 0.0, 0.0 },
	{ 0.0, 1.0, 0.0, 0.0 },
	{ 0.0, 0.0, 1.0, 0.0 },
	{ 0.0, 0.0, 0.0, 1.0 }
} };

Matrix4x4 toMatrix(const Dot3<double>& v) {
	Matrix4x4 result = {};
	result.m[0][3] = v.x;
	result.m[1][3] = v.y;
	result.m[2][3] = v.z;
	result.m[3][3] = 1.0;
	return result;
}
double FOCUS_DISTANCE = 1.0;
double CUT_SIZE = 0.1;

bool isDotBehind(Dot3<double> tested) {
	return tested.z <= CUT_SIZE;
}

Dot3<double> vertexShader(Dot3<double> vertex, Matrix4x4& CameraPos) {
	Matrix4x4 vertexMatrix = toMatrix(vertex);
	Matrix4x4 transformedMatrix = CameraPos * vertexMatrix;
	return {FOCUS_DISTANCE * transformedMatrix.m[0][3], FOCUS_DISTANCE * transformedMatrix.m[1][3], transformedMatrix.m[2][3]};
}
Dot3<double> linearIntersection(Dot3<double> dot1, Dot3<double> dot2) {
	double ratio = (dot1.z == dot2.z)?INFINITY:(dot1.z - CUT_SIZE)/(dot1.z - dot2.z);
	return Dot3<double>{dot1.x + (dot2.x - dot1.x) * ratio, dot1.y + (dot2.y - dot1.y) * ratio, CUT_SIZE};
}
struct Triangle { Dot3<double> dot1; Dot3<double> dot2; Dot3<double> dot3; };
Triangle triangleRearrange(Triangle target) {
	double dot1z = target.dot1.z;
	double dot2z = target.dot2.z;
	double dot3z = target.dot3.z;
	Triangle result;
	if ((dot3z >= dot2z) && (dot3z >= dot1z)) {
		result.dot3 = target.dot3;
		if (dot2z >= dot1z) {
			result.dot1 = target.dot1;
			result.dot2 = target.dot2;
		}
		else {
			result.dot1 = target.dot2;
			result.dot2 = target.dot1;
		}
	}
	else if ((dot2z >= dot1z) && (dot2z >= dot3z)) {
		result.dot3 = target.dot2;
		if (dot3z >= dot1z) {
			result.dot1 = target.dot1;
			result.dot2 = target.dot3;
		}
		else {
			result.dot1 = target.dot3;
			result.dot2 = target.dot1;
		}
	}
	else {
		result.dot3 = target.dot1;
		if (dot2z >= dot3z) {
			result.dot1 = target.dot3;
			result.dot2 = target.dot2;
		}
		else {
			result.dot1 = target.dot2;
			result.dot2 = target.dot3;
		}
	}
	return result;
}
void triangleCut() {
	Dot3<double> dot1 = *(triangleOrthoBuffer + triangleCurPtr + 0);
	Dot3<double> dot2 = *(triangleOrthoBuffer + triangleCurPtr + 1);
	Dot3<double> dot3 = *(triangleOrthoBuffer + triangleCurPtr + 2);
	Triangle arrangedTriangle = triangleRearrange(Triangle{ dot1, dot2, dot3 });
	dot1 = arrangedTriangle.dot1;
	dot2 = arrangedTriangle.dot2;
	dot3 = arrangedTriangle.dot3;
	bool isDot1Invisible = isDotBehind(dot1);
	bool isDot2Invisible = isDotBehind(dot2);
	bool isDot3Invisible = isDotBehind(dot3);
	if (isDot3Invisible) { memcpy((void*)(triangleOrthoBuffer + triangleCurPtr), (void*)(triangleOrthoBuffer + triangleEndPtr), sizeof(Triangle)); triangleCurPtr-=3; triangleEndPtr-=3; }
	else if (isDot2Invisible) {
		dot1 = linearIntersection(dot1, dot3); dot2 = linearIntersection(dot2, dot3); triangleOrthoBuffer[triangleCurPtr] = dot1; triangleOrthoBuffer[triangleCurPtr + 1] = dot2; triangleOrthoBuffer[triangleCurPtr + 2] = dot3;
	}
	else if (isDot1Invisible) {
		Dot3<double> dot4 = linearIntersection(dot1, dot3); Dot3<double>dot5 = linearIntersection(dot1, dot2);
		triangleOrthoBuffer[triangleCurPtr] = dot4; triangleOrthoBuffer[triangleCurPtr+1] = dot2; triangleOrthoBuffer[triangleCurPtr+2] = dot3;
		triangleOrthoBuffer[triangleEndPtr  + 3] = dot4; triangleOrthoBuffer[triangleEndPtr + 4] = dot5; triangleOrthoBuffer[triangleEndPtr + 5] = dot3;
		triangleEndPtr += 3;
	}
}

void fragmentShaderTriangle(){
	for (triangleCurPtr = 0; triangleCurPtr < triangleEndPtr + 3; triangleCurPtr ++) {
		triangleOrthoBuffer[triangleCurPtr] = vertexShader(triangleBuffer[triangleCurPtr], testCameraPos);
	}
	triangleCurPtr = 0;
	while (triangleCurPtr <= triangleEndPtr) {
		triangleCut();
		triangleCurPtr += 3;
	}
	for (triangleCurPtr = 0; triangleCurPtr < triangleEndPtr + 3; triangleCurPtr++) {
		triangleOrthoBuffer[triangleCurPtr].x = triangleOrthoBuffer[triangleCurPtr].x / triangleOrthoBuffer[triangleCurPtr].z;
		triangleOrthoBuffer[triangleCurPtr].y = triangleOrthoBuffer[triangleCurPtr].y / triangleOrthoBuffer[triangleCurPtr].z;
	}
}
int main() {

	cout << "Original Vertex: (" << testVertex.x << ", " << testVertex.y << ", " << testVertex.z << ")\n";
	Dot3<double> transformedVertex = vertexShader(testVertex, testCameraPos);
	cout << "Transformed Vertex: (" << transformedVertex.x << ", " << transformedVertex.y << ", " << transformedVertex.z << ")\n";
}




