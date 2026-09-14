#pragma once
#include "m3d.hpp"
#include <algorithm>
#include <cmath>
namespace math3d {
using m3d::Vec3;
inline Vec3 operator+(Vec3 a,Vec3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(Vec3 a,float f) {return {a.x*f,a.y*f,a.z*f};}
inline float dot(Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float length(Vec3 a) {return std::sqrt(dot(a,a));}
inline Vec3 normal(Vec3 a) {return a*(1/std::max(1e-10f,length(a)));}
struct Mat4 {float v[16]{};static Mat4 identity() {Mat4 m;for(int i=0;i<4;++i)m.v[i*5]=1;return m;}};
inline Mat4 operator*(const Mat4& a,const Mat4& b) {
    Mat4 c;for(int col=0;col<4;++col)for(int row=0;row<4;++row)for(int k=0;k<4;++k)c.v[col*4+row]+=a.v[k*4+row]*b.v[col*4+k];return c;
}
inline Vec3 transform(const Mat4& m,Vec3 p) {return {m.v[0]*p.x+m.v[4]*p.y+m.v[8]*p.z+m.v[12],m.v[1]*p.x+m.v[5]*p.y+m.v[9]*p.z+m.v[13],m.v[2]*p.x+m.v[6]*p.y+m.v[10]*p.z+m.v[14]};}
inline Mat4 instance(const prj::Document& d,unsigned i) {
    constexpr float angle=6.28318530718f/4096;
    float x=int32_t(d.field(i,0x1c))*angle,y=-int32_t(d.field(i,0x20))*angle,z=int32_t(d.field(i,0x24))*angle;
    float cx=std::cos(x),sx=std::sin(x),cy=std::cos(y),sy=std::sin(y),cz=std::cos(z),sz=std::sin(z);
    Mat4 m=Mat4::identity();
    m.v[0]=cz*cy;m.v[1]=sz*cy;m.v[2]=-sy;
    m.v[4]=cz*sx*sy-sz*cx;m.v[5]=sz*sx*sy+cz*cx;m.v[6]=cy*sx;
    m.v[8]=cz*cx*sy+sz*sx;m.v[9]=sz*cx*sy-cz*sx;m.v[10]=cy*cx;
    m.v[12]=int32_t(d.field(i,0x10))/1024.0f;m.v[13]=int32_t(d.field(i,0x14))/1024.0f;m.v[14]=int32_t(d.field(i,0x18))/1024.0f;return m;
}
inline Mat4 perspective(float fovy,float aspect,float near,float far) {
    Mat4 m;float f=1/std::tan(fovy/2);m.v[0]=f/aspect;m.v[5]=f;m.v[10]=(far+near)/(near-far);m.v[11]=-1;m.v[14]=2*far*near/(near-far);return m;
}
struct CameraBasis {Vec3 forward,right,up;};
inline CameraBasis camera_basis(Vec3 eye,Vec3 target) {
    // Dark Omen uses Direct3D's left-handed world: looking along +Z,
    // +X must appear on the right. Retain this basis for rendering AND picking.
    Vec3 f=normal(target-eye),r=normal(cross({0,1,0},f));
    return {f,r,cross(f,r)};
}
inline Mat4 look_at(Vec3 eye,Vec3 target) {
    auto basis=camera_basis(eye,target);Vec3 f=basis.forward,s=basis.right,u=basis.up;
    // OpenGL clip space still expects visible geometry on negative view Z.
    // The resulting reflection is the Direct3D-world -> OpenGL-view conversion.
    Mat4 m=Mat4::identity();
    m.v[0]=s.x;m.v[4]=s.y;m.v[8]=s.z;m.v[1]=u.x;m.v[5]=u.y;m.v[9]=u.z;m.v[2]=-f.x;m.v[6]=-f.y;m.v[10]=-f.z;
    m.v[12]=-dot(s,eye);m.v[13]=-dot(u,eye);m.v[14]=dot(f,eye);return m;
}
inline bool ray_triangle(Vec3 origin,Vec3 direction,Vec3 a,Vec3 b,Vec3 c,float& distance) {
    Vec3 e1=b-a,e2=c-a,h=cross(direction,e2);float det=dot(e1,h);if(std::abs(det)<1e-7f)return false;
    float inv=1/det;Vec3 s=origin-a;float u=inv*dot(s,h);if(u<0 || u>1)return false;
    Vec3 q=cross(s,e1);float v=inv*dot(direction,q);if(v<0 || u+v>1)return false;
    float t=inv*dot(e2,q);if(t<=0 || t>=distance)return false;distance=t;return true;
}
}
