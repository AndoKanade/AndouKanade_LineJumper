#pragma once

#include <cmath>

// =============================================================================
// ベクトル構造体
// =============================================================================

struct Vector2{
    float x,y;

    constexpr Vector2& operator+=(const Vector2& other){
        x += other.x;
        y += other.y;
        return *this;
    }
};

struct Vector3{
    float x,y,z;

    // エディターの編集履歴で、変更があったかを比べるのに使う
    bool operator==(const Vector3&) const = default;
};

struct Vector4{
    float x,y,z,w;
};

// =============================================================================
// 行列構造体
// =============================================================================

struct Matrix4x4{
    float m[4][4];
};

struct Matrix3x3{
    float m[3][3];
};

// =============================================================================
// 変換構造体
// =============================================================================

struct Transform{
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

struct EulerTransform{
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

struct TransformationMatrix{
    Matrix4x4 WVP;
    Matrix4x4 World;
};

// =============================================================================
// クォータニオン構造体
// =============================================================================

struct Quaternion{
    float x,y,z,w;

    constexpr Quaternion operator-() const{
        return {-x, -y, -z, -w};
    }

    constexpr Quaternion operator+(const Quaternion& other) const{
        return {x + other.x, y + other.y, z + other.z, w + other.w};
    }

    constexpr Quaternion operator*(float scalar) const{
        return {x * scalar, y * scalar, z * scalar, w * scalar};
    }
};

struct QuaternionTransform{
    Vector3 scale;
    Quaternion rotate;
    Vector3 translate;
};

// =============================================================================
// ベクトル演算
// =============================================================================

constexpr bool operator<(const Vector2& a,const Vector2& b){
    if(a.x != b.x) return a.x < b.x;
    return a.y < b.y;
}

constexpr bool operator!=(const Vector2& a,const Vector2& b){
    return a.x != b.x || a.y != b.y;
}

constexpr Vector3 operator-(const Vector3& v1,const Vector3& v2){
    return {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

constexpr float Dot(const Vector3& v1,const Vector3& v2){
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

constexpr Vector3 Cross(const Vector3& v1,const Vector3& v2){
    return {
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x
    };
}

inline Vector3 Normalize(const Vector3& v){
    Vector3 result = {0, 0, 0};
    float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

    if(length != 0.0f){
        result.x = v.x / length;
        result.y = v.y / length;
        result.z = v.z / length;
    }
    return result;
}

constexpr bool operator<(const Vector3& a,const Vector3& b){
    if(a.x != b.x) return a.x < b.x;
    if(a.y != b.y) return a.y < b.y;
    return a.z < b.z;
}

constexpr bool operator!=(const Vector3& a,const Vector3& b){
    return a.x != b.x || a.y != b.y || a.z != b.z;
}

constexpr Vector3& operator+=(Vector3& lhv,const Vector3& rhv){
    lhv.x += rhv.x;
    lhv.y += rhv.y;
    lhv.z += rhv.z;
    return lhv;
}

constexpr Vector3 operator+(const Vector3& v1,const Vector3& v2){
    return {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
}

constexpr Vector3 operator-(const Vector3& v){
    return {-v.x, -v.y, -v.z};
}

constexpr Vector3 operator*(const Vector3& v,float s){
    return {v.x * s, v.y * s, v.z * s};
}

constexpr Vector3 operator*(float s,const Vector3& v){
    return {v.x * s, v.y * s, v.z * s};
}

constexpr Vector3 operator/(const Vector3& v,float s){
    return {v.x / s, v.y / s, v.z / s};
}

constexpr bool operator<(const Vector4& a,const Vector4& b){
    if(a.x != b.x) return a.x < b.x;
    if(a.y != b.y) return a.y < b.y;
    if(a.z != b.z) return a.z < b.z;
    return a.w < b.w;
}

constexpr bool operator!=(const Vector4& a,const Vector4& b){
    return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w;
}

inline float Length(const Vector3& v){
    return std::sqrt(Dot(v,v));
}

inline float Distance(const Vector3& v1,const Vector3& v2){
    return Length(v1 - v2);
}

constexpr Vector3 Lerp(const Vector3& v1,const Vector3& v2,float t){
    return v1 + (v2 - v1) * t;
}

constexpr float Lerp(float a,float b,float t){
    return a + (b - a) * t;
}

inline Vector3 CatmullRom(const Vector3& p0,const Vector3& p1,const Vector3& p2,const Vector3& p3,float t){
    float t2 = t * t;
    float t3 = t2 * t;
    return (
        (p1 * 2.0f) +
        (-p0 + p2) * t +
        (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
        (-p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3
        ) * 0.5f;
}

inline float AngleBetween(const Vector3& v1,const Vector3& v2){
    float dot = Dot(Normalize(v1),Normalize(v2));
    if(dot > 1.0f) dot = 1.0f;
    if(dot < -1.0f) dot = -1.0f;
    return std::acos(dot);
}

// =============================================================================
// 行列演算
// =============================================================================

constexpr Matrix4x4 MakeIdentity4x4(){
    Matrix4x4 result{};
    for(int i = 0; i < 4; i++){
        for(int j = 0; j < 4; j++){
            if(i == j){
                result.m[i][j] = 1.0f;
            } else{
                result.m[i][j] = 0.0f;
            }
        }
    }
    return result;
}

constexpr Matrix4x4 MakeScaleMatrix(const Vector3& scale){
    Matrix4x4 matrix = {};
    matrix.m[0][0] = scale.x;
    matrix.m[1][1] = scale.y;
    matrix.m[2][2] = scale.z;
    matrix.m[3][3] = 1.0f;
    return matrix;
}

constexpr Matrix4x4 MakeTranslateMatrix(const Vector3& translate){
    Matrix4x4 matrix = {};
    matrix.m[0][0] = 1.0f;
    matrix.m[1][1] = 1.0f;
    matrix.m[2][2] = 1.0f;
    matrix.m[3][3] = 1.0f;
    matrix.m[3][0] = translate.x;
    matrix.m[3][1] = translate.y;
    matrix.m[3][2] = translate.z;
    return matrix;
}

inline Matrix4x4 MakeRotateXMatrix(float radian){
    Matrix4x4 result{};
    result.m[0][0] = 1.0f;
    result.m[3][3] = 1.0f;
    result.m[1][1] = std::cos(radian);
    result.m[1][2] = std::sin(radian);
    result.m[2][1] = -std::sin(radian);
    result.m[2][2] = std::cos(radian);
    return result;
}

inline Matrix4x4 MakeRotateYMatrix(float radian){
    Matrix4x4 result{};
    result.m[1][1] = 1.0f;
    result.m[3][3] = 1.0f;
    result.m[0][0] = std::cos(radian);
    result.m[0][2] = -std::sin(radian);
    result.m[2][0] = std::sin(radian);
    result.m[2][2] = std::cos(radian);
    return result;
}

inline Matrix4x4 MakeRotateZMatrix(float radian){
    Matrix4x4 result{};
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;
    result.m[0][0] = std::cos(radian);
    result.m[0][1] = std::sin(radian);
    result.m[1][0] = -std::sin(radian);
    result.m[1][1] = std::cos(radian);
    return result;
}

constexpr Matrix4x4 Multiply(const Matrix4x4& m1,const Matrix4x4& m2){
    Matrix4x4 result{};
    for(int row = 0; row < 4; ++row){
        for(int col = 0; col < 4; ++col){
            result.m[row][col] = 0.0f;
            for(int k = 0; k < 4; ++k){
                result.m[row][col] += m1.m[row][k] * m2.m[k][col];
            }
        }
    }
    return result;
}

inline Matrix4x4 MakeAffineMatrix(const Vector3& scale,const Vector3& rotate,const Vector3& translate){
    Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
    Matrix4x4 rotateX = MakeRotateXMatrix(rotate.x);
    Matrix4x4 rotateY = MakeRotateYMatrix(rotate.y);
    Matrix4x4 rotateZ = MakeRotateZMatrix(rotate.z);

    Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX,rotateY),rotateZ);
    Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

    return Multiply(Multiply(scaleMatrix,rotateMatrix),translateMatrix);
}

inline Matrix4x4 MakeAffineMatrix(const Vector3& scale,const Matrix4x4& rotate,const Vector3& translate){
    Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
    Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

    // S * R * T の順で計算
    return Multiply(Multiply(scaleMatrix,rotate),translateMatrix);
}

inline Matrix4x4 MakePerspectiveFovMatrix(float fovY,float aspectRatio,float nearClip,float farClip){
    float f = 1.0f / std::tan(fovY * 0.5f);
    float range = farClip / (farClip - nearClip);

    Matrix4x4 result = {};
    result.m[0][0] = f / aspectRatio;
    result.m[1][1] = f;
    result.m[2][2] = range;
    result.m[2][3] = 1.0f;
    result.m[3][2] = -range * nearClip;
    result.m[3][3] = 0.0f;

    return result;
}

constexpr Matrix4x4 Inverse(const Matrix4x4& m){
    Matrix4x4 result{};

    float det =
        m.m[0][0] * (m.m[1][1] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) -
            m.m[1][2] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) +
            m.m[1][3] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1])) -
        m.m[0][1] * (m.m[1][0] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) -
            m.m[1][2] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) +
            m.m[1][3] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0])) +
        m.m[0][2] * (m.m[1][0] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) -
            m.m[1][1] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) +
            m.m[1][3] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) -
        m.m[0][3] * (m.m[1][0] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1]) -
            m.m[1][1] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0]) +
            m.m[1][2] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0]));

    if(det == 0){
        return result;
    }

    float invDet = 1.0f / det;

    result.m[0][0] = (m.m[1][1] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) - m.m[1][2] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) + m.m[1][3] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1])) * invDet;
    result.m[0][1] = (-m.m[0][1] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) + m.m[0][2] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) - m.m[0][3] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1])) * invDet;
    result.m[0][2] = (m.m[0][1] * (m.m[1][2] * m.m[3][3] - m.m[1][3] * m.m[3][2]) - m.m[0][2] * (m.m[1][1] * m.m[3][3] - m.m[1][3] * m.m[3][1]) + m.m[0][3] * (m.m[1][1] * m.m[3][2] - m.m[1][2] * m.m[3][1])) * invDet;
    result.m[0][3] = (-m.m[0][1] * (m.m[1][2] * m.m[2][3] - m.m[1][3] * m.m[2][2]) + m.m[0][2] * (m.m[1][1] * m.m[2][3] - m.m[1][3] * m.m[2][1]) - m.m[0][3] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1])) * invDet;

    result.m[1][0] = (-m.m[1][0] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) + m.m[1][2] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) - m.m[1][3] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0])) * invDet;
    result.m[1][1] = (m.m[0][0] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) - m.m[0][2] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) + m.m[0][3] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0])) * invDet;
    result.m[1][2] = -(m.m[0][0] * (m.m[1][2] * m.m[3][3] - m.m[1][3] * m.m[3][2]) - m.m[0][2] * (m.m[1][0] * m.m[3][3] - m.m[1][3] * m.m[3][0]) + m.m[0][3] * (m.m[1][0] * m.m[3][2] - m.m[1][2] * m.m[3][0])) * invDet;
    result.m[1][3] = (m.m[0][0] * (m.m[1][2] * m.m[2][3] - m.m[1][3] * m.m[2][2]) - m.m[0][2] * (m.m[1][0] * m.m[2][3] - m.m[1][3] * m.m[2][0]) + m.m[0][3] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0])) * invDet;

    result.m[2][0] = (m.m[1][0] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) - m.m[1][1] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) + m.m[1][3] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) * invDet;
    result.m[2][1] = (-m.m[0][0] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) + m.m[0][1] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) - m.m[0][3] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) * invDet;
    result.m[2][2] = (m.m[0][0] * (m.m[1][1] * m.m[3][3] - m.m[1][3] * m.m[3][1]) - m.m[0][1] * (m.m[1][0] * m.m[3][3] - m.m[1][3] * m.m[3][0]) + m.m[0][3] * (m.m[1][0] * m.m[3][1] - m.m[1][1] * m.m[3][0])) * invDet;
    result.m[2][3] = (-m.m[0][0] * (m.m[1][1] * m.m[2][3] - m.m[1][3] * m.m[2][1]) + m.m[0][1] * (m.m[1][0] * m.m[2][3] - m.m[1][3] * m.m[2][0]) - m.m[0][3] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0])) * invDet;

    result.m[3][0] = (-m.m[1][0] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1]) + m.m[1][1] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0]) - m.m[1][2] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) * invDet;
    result.m[3][1] = (m.m[0][0] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1]) - m.m[0][1] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0]) + m.m[0][2] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) * invDet;
    result.m[3][2] = (-m.m[0][0] * (m.m[1][1] * m.m[3][2] - m.m[1][2] * m.m[3][1]) + m.m[0][1] * (m.m[1][0] * m.m[3][2] - m.m[1][2] * m.m[3][0]) - m.m[0][2] * (m.m[1][0] * m.m[3][1] - m.m[1][1] * m.m[3][0])) * invDet;
    result.m[3][3] = (m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) - m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) + m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0])) * invDet;

    return result;
}

constexpr Matrix4x4 MakeOrthographicMatrix(float left,float top,float right,float bottom,float nearClip,float farClip){
    Matrix4x4 result = {};

    result.m[0][0] = 2.0f / (right - left);
    result.m[1][1] = 2.0f / (top - bottom);
    result.m[2][2] = 1.0f / (farClip - nearClip);
    result.m[3][0] = (left + right) / (left - right);
    result.m[3][1] = (top + bottom) / (bottom - top);
    result.m[3][2] = -nearClip / (farClip - nearClip);
    result.m[3][3] = 1.0f;

    return result;
}

constexpr Matrix4x4 Transpose(const Matrix4x4& m){
    Matrix4x4 result{};
    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            result.m[i][j] = m.m[j][i];
        }
    }
    return result;
}

// =============================================================================
// クォータニオン演算
// =============================================================================

constexpr float Dot(const Quaternion& q1,const Quaternion& q2){
    return q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
}

constexpr Quaternion operator*(float scalar,const Quaternion& quat){
    return {quat.x * scalar, quat.y * scalar, quat.z * scalar, quat.w * scalar};
}

inline Quaternion Slerp(const Quaternion& q1,const Quaternion& q2,float t){
    Quaternion result;
    float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;

    Quaternion targetQ2 = q2;
    if(dot < 0.0f){
        targetQ2.x = -q2.x;
        targetQ2.y = -q2.y;
        targetQ2.z = -q2.z;
        targetQ2.w = -q2.w;
        dot = -dot;
    }

    if(dot > 0.9995f){
        result.x = q1.x + t * (targetQ2.x - q1.x);
        result.y = q1.y + t * (targetQ2.y - q1.y);
        result.z = q1.z + t * (targetQ2.z - q1.z);
        result.w = q1.w + t * (targetQ2.w - q1.w);

        float len = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
        if(len != 0.0f){
            result.x /= len;
            result.y /= len;
            result.z /= len;
            result.w /= len;
        }
        return result;
    }

    float theta_0 = std::acos(dot);
    float theta = theta_0 * t;
    float sin_theta = std::sin(theta);
    float sin_theta_0 = std::sin(theta_0);

    float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
    float s1 = sin_theta / sin_theta_0;

    result.x = s0 * q1.x + s1 * targetQ2.x;
    result.y = s0 * q1.y + s1 * targetQ2.y;
    result.z = s0 * q1.z + s1 * targetQ2.z;
    result.w = s0 * q1.w + s1 * targetQ2.w;

    return result;
}

constexpr Matrix4x4 MakeRotateMatrix(const Quaternion& q){
    Matrix4x4 result = MakeIdentity4x4();

    result.m[0][0] = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    result.m[0][1] = 2.0f * (q.x * q.y + q.w * q.z);
    result.m[0][2] = 2.0f * (q.x * q.z - q.w * q.y);

    result.m[1][0] = 2.0f * (q.x * q.y - q.w * q.z);
    result.m[1][1] = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    result.m[1][2] = 2.0f * (q.y * q.z + q.w * q.x);

    result.m[2][0] = 2.0f * (q.x * q.z + q.w * q.y);
    result.m[2][1] = 2.0f * (q.y * q.z - q.w * q.x);
    result.m[2][2] = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

    return result;
}

constexpr Matrix4x4 MakeRotateQuaternionMatrix(const Quaternion& q){
    Matrix4x4 result{};

    float x = q.x;
    float y = q.y;
    float z = q.z;
    float w = q.w;

    result.m[0][0] = 1.0f - 2.0f * (y * y + z * z);
    result.m[0][1] = 2.0f * (x * y + z * w);
    result.m[0][2] = 2.0f * (x * z - y * w);
    result.m[0][3] = 0.0f;

    result.m[1][0] = 2.0f * (x * y - z * w);
    result.m[1][1] = 1.0f - 2.0f * (x * x + z * z);
    result.m[1][2] = 2.0f * (y * z + x * w);
    result.m[1][3] = 0.0f;

    result.m[2][0] = 2.0f * (x * z + y * w);
    result.m[2][1] = 2.0f * (y * z - x * w);
    result.m[2][2] = 1.0f - 2.0f * (x * x + y * y);
    result.m[2][3] = 0.0f;

    result.m[3][0] = 0.0f;
    result.m[3][1] = 0.0f;
    result.m[3][2] = 0.0f;
    result.m[3][3] = 1.0f;

    return result;
}

constexpr Matrix4x4 MakeAffineMatrixQuaternion(const Vector3& scale,const Quaternion& rotate,const Vector3& translate){
    Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
    Matrix4x4 rotateMatrix = MakeRotateQuaternionMatrix(rotate);
    Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

    return Multiply(Multiply(scaleMatrix,rotateMatrix),translateMatrix);
}

inline Quaternion MakeQuaternionFromEuler(float x,float y,float z){
    float cx = std::cos(x * 0.5f);
    float sx = std::sin(x * 0.5f);
    float cy = std::cos(y * 0.5f);
    float sy = std::sin(y * 0.5f);
    float cz = std::cos(z * 0.5f);
    float sz = std::sin(z * 0.5f);

    Quaternion q;
    q.x = sx * cy * cz - cx * sy * sz;
    q.y = cx * sy * cz + sx * cy * sz;
    q.z = cx * cy * sz - sx * sy * cz;
    q.w = cx * cy * cz + sx * sy * sz;
    return q;
}

inline Quaternion Normalize(const Quaternion& q){
    Quaternion result = q;
    float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);

    if(length != 0.0f){
        result.x /= length;
        result.y /= length;
        result.z /= length;
        result.w /= length;
    }
    return result;
}