#include <iostream>
#include <string>
#include <windows.h>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <time.h>
#include <algorithm>
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 60
#define MAX_TRIANGLE_QUANTITY 1000
#define MAX_CLIPPED_TRIANGLE_QUANTITY (MAX_TRIANGLE_QUANTITY * 2)
#define EPSILON 1e-6
using namespace std;

char screenBuffer[SCREEN_HEIGHT][SCREEN_WIDTH];
double depthBuffer[SCREEN_HEIGHT][SCREEN_WIDTH];
char curLineChar = '#';
char curSurfaceChar = '*';

template <typename T>
struct Dot3 { T x, y, z; };

int triangleCurPtr = 0;
int triangleEndPtr = -3;          // 裁剪后的最后一个三角形起始索引
int rawTriangleEndPtr = -3;       // 原始最后一个三角形起始索引
int triangleQuantity = 0;         // 原始三角形数量

Dot3<double>* triangleBuffer = (Dot3<double>*)malloc(sizeof(Dot3<double>) * MAX_TRIANGLE_QUANTITY * 3);
Dot3<double>* triangleOrthoBuffer = (Dot3<double>*)malloc(sizeof(Dot3<double>) * MAX_CLIPPED_TRIANGLE_QUANTITY * 3);
char triangleBufferChar[MAX_TRIANGLE_QUANTITY];
char triangleOrthoBufferChar[MAX_CLIPPED_TRIANGLE_QUANTITY];

Dot3<double> operator+(const Dot3<double>& a, const Dot3<double>& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}
Dot3<double> operator-(const Dot3<double>& a, const Dot3<double>& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}
Dot3<double> operator*(const Dot3<double>& a, double b) {
    return { a.x * b, a.y * b, a.z * b };
}
Dot3<double> operator/(const Dot3<double>& a, double b) {
    return { a.x / b, a.y / b, a.z / b };
}
Dot3<double> operator^(const Dot3<double>& a, const Dot3<double>& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
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

Matrix4x4 operator+(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = a.m[i][j] + b.m[i][j];
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
    return {
        FOCUS_DISTANCE * transformedMatrix.m[0][3],
        FOCUS_DISTANCE * transformedMatrix.m[1][3],
        transformedMatrix.m[2][3]
    };
}

Dot3<double> linearIntersection(Dot3<double> dot1, Dot3<double> dot2) {
    double dz = dot1.z - dot2.z;
    double ratio = (fabs(dz) < EPSILON) ? 0.0 : (dot1.z - CUT_SIZE) / dz;
    return Dot3<double>{
        dot1.x + (dot2.x - dot1.x) * ratio,
            dot1.y + (dot2.y - dot1.y) * ratio,
            CUT_SIZE
    };
}

struct Triangle {
    Dot3<double> dot1;
    Dot3<double> dot2;
    Dot3<double> dot3;
};

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
    Dot3<double> dot1 = triangleOrthoBuffer[triangleCurPtr + 0];
    Dot3<double> dot2 = triangleOrthoBuffer[triangleCurPtr + 1];
    Dot3<double> dot3 = triangleOrthoBuffer[triangleCurPtr + 2];

    Triangle arrangedTriangle = triangleRearrange(Triangle{ dot1, dot2, dot3 });
    dot1 = arrangedTriangle.dot1;
    dot2 = arrangedTriangle.dot2;
    dot3 = arrangedTriangle.dot3;

    bool isDot1Invisible = isDotBehind(dot1);
    bool isDot2Invisible = isDotBehind(dot2);
    bool isDot3Invisible = isDotBehind(dot3);

    if (isDot3Invisible) {
        // 整个三角形不可见，用最后一个三角形覆盖当前三角形
        triangleOrthoBufferChar[triangleCurPtr / 3] = triangleOrthoBufferChar[triangleEndPtr / 3];
        memcpy((void*)(triangleOrthoBuffer + triangleCurPtr),
            (void*)(triangleOrthoBuffer + triangleEndPtr),
            sizeof(Triangle));
        triangleCurPtr -= 3;
        triangleEndPtr -= 3;
    }
    else if (isDot2Invisible) {
        // dot1, dot2 不可见，dot3 可见，裁剪成一个三角形
        dot1 = linearIntersection(dot1, dot3);
        dot2 = linearIntersection(dot2, dot3);
        triangleOrthoBuffer[triangleCurPtr] = dot1;
        triangleOrthoBuffer[triangleCurPtr + 1] = dot2;
        triangleOrthoBuffer[triangleCurPtr + 2] = dot3;
    }
    else if (isDot1Invisible) {
        // 只有 dot1 不可见，裁剪成两个三角形
        Dot3<double> dot4 = linearIntersection(dot1, dot2);
        Dot3<double> dot5 = linearIntersection(dot1, dot3);

        triangleOrthoBuffer[triangleCurPtr] = dot4;
        triangleOrthoBuffer[triangleCurPtr + 1] = dot2;
        triangleOrthoBuffer[triangleCurPtr + 2] = dot3;

        int newTriStart = triangleEndPtr + 3;
        triangleOrthoBuffer[newTriStart] = dot4;
        triangleOrthoBuffer[newTriStart + 1] = dot5;
        triangleOrthoBuffer[newTriStart + 2] = dot3;
        triangleOrthoBufferChar[newTriStart / 3] = triangleOrthoBufferChar[triangleCurPtr / 3];

        triangleEndPtr += 3;
    }
}

void fillTriangle(Triangle Target, char fillChar) {
    Dot3<double> dot1 = Target.dot1;
    Dot3<double> dot2 = Target.dot2;
    Dot3<double> dot3 = Target.dot3;

    // compute bounding box
    int minX = (int)floor(min(dot1.x, min(dot2.x, dot3.x)));
    int maxX = (int)ceil(max(dot1.x, max(dot2.x, dot3.x)));
    int minY = (int)floor(min(dot1.y, min(dot2.y, dot3.y)));
    int maxY = (int)ceil(max(dot1.y, max(dot2.y, dot3.y)));

    // clamp bounding box to screen
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= SCREEN_WIDTH) maxX = SCREEN_WIDTH - 1;
    if (maxY >= SCREEN_HEIGHT) maxY = SCREEN_HEIGHT - 1;

    if (minX > maxX || minY > maxY) return;

    double denom = ((dot2.y - dot3.y) * (dot1.x - dot3.x) + (dot3.x - dot2.x) * (dot1.y - dot3.y));
    if (fabs(denom) < EPSILON) return;

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            double alpha = ((dot2.y - dot3.y) * (x - dot3.x) + (dot3.x - dot2.x) * (y - dot3.y)) / denom;
            double beta = ((dot3.y - dot1.y) * (x - dot3.x) + (dot1.x - dot3.x) * (y - dot3.y)) / denom;
            double gamma = 1.0 - alpha - beta;

            if (alpha >= 0 && beta >= 0 && gamma >= 0 && alpha <= 1 && beta <= 1 && gamma <= 1) {
                double z = alpha * dot1.z + beta * dot2.z + gamma * dot3.z;
                if (z < depthBuffer[y][x]) {
                    depthBuffer[y][x] = z;
                    screenBuffer[y][x] = fillChar;
                }
            }
        }
    }
}

void fragmentShaderTriangle() {
    // 从原始三角形复制到裁剪缓冲区
    for (triangleCurPtr = 0; triangleCurPtr < rawTriangleEndPtr + 3; triangleCurPtr++) {
        triangleOrthoBuffer[triangleCurPtr] = vertexShader(triangleBuffer[triangleCurPtr], testCameraPos);
    }

    // 复制原始三角形字符
    for (int i = 0; i < triangleQuantity; i++) {
        triangleOrthoBufferChar[i] = triangleBufferChar[i];
    }

    // 每帧从原始末尾重新开始裁剪
    triangleEndPtr = rawTriangleEndPtr;
    triangleCurPtr = 0;

    while (triangleCurPtr <= triangleEndPtr) {
        triangleCut();
        triangleCurPtr += 3;
    }

    // 透视除法
    for (triangleCurPtr = 0; triangleCurPtr < triangleEndPtr + 3; triangleCurPtr++) {
        if (fabs(triangleOrthoBuffer[triangleCurPtr].z) > EPSILON) {
            triangleOrthoBuffer[triangleCurPtr].x = triangleOrthoBuffer[triangleCurPtr].x / triangleOrthoBuffer[triangleCurPtr].z;
            triangleOrthoBuffer[triangleCurPtr].y = triangleOrthoBuffer[triangleCurPtr].y / triangleOrthoBuffer[triangleCurPtr].z;
        }
    }

    // 光栅化
    for (triangleCurPtr = 0; triangleCurPtr < triangleEndPtr + 3; triangleCurPtr += 3) {
        fillTriangle(
            Triangle{
                triangleOrthoBuffer[triangleCurPtr],
                triangleOrthoBuffer[triangleCurPtr + 1],
                triangleOrthoBuffer[triangleCurPtr + 2]
            },
            triangleOrthoBufferChar[triangleCurPtr / 3]
        );
    }
}

void addTriangle(Dot3<double> dot1, Dot3<double> dot2, Dot3<double> dot3, char surfaceChar) {
    triangleBuffer[rawTriangleEndPtr + 3] = dot1;
    triangleBuffer[rawTriangleEndPtr + 4] = dot2;
    triangleBuffer[rawTriangleEndPtr + 5] = dot3;

    triangleBufferChar[triangleQuantity] = surfaceChar;
    triangleQuantity++;

    rawTriangleEndPtr += 3;
}

void clearScreenBuffer() {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screenBuffer[y][x] = ' ';
            depthBuffer[y][x] = INFINITY;
        }
    }
}

void renderScreenBuffer() {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            cout << screenBuffer[y][x] << " ";
        }
        cout << endl;
    }
}

int main() {
    addTriangle({ 40.0, 30.0, -2.0 }, { 30.0, 10.0, 1.0 }, { 20.0, 30.0, 1.0 }, '#');

    while (true) {
        clearScreenBuffer();
        system("cls");
        fragmentShaderTriangle();
        renderScreenBuffer();
        Sleep(33);
    }

    return 0;
}