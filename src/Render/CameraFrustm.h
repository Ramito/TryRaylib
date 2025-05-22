#pragma once
#include <raylib.h>

struct CameraFrustum
{
    Vector3 Target;
    float TopSupport;
    Vector3 TopNormal;
    float LeftSupport;
    Vector3 LeftNormal;
    float BottomSupport;
    Vector3 BottomNormal;
    float RightSupport;
    Vector3 RightNormal;
};
