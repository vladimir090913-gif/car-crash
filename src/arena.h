#pragma once
#include "softbody.h"
#include "flexmesh.h"
using namespace sb;

// Ящик = 8-узловое soft-body (тоже ломается)
inline void makeCrate(Body& B, V3 c, float s, float mass, Color col) {
    B = Body{};
    for (int i=0;i<8;i++) {
        Node n;
        n.p0 = n.p = n.pp = c + V3((i&1? s:-s), (i&2? s:-s), (i&4? s:-s));
        n.w = 8.f/mass; n.r = 0.09f; n.fric = 0.7f;
        B.nodes.push_back(n);
    }
    for (int a=0;a<8;a++) for (int b=a+1;b<8;b++)
        B.addBeam(a,b, 1.6e6f, 220.f, STRUCT, 0.05f, 0.40f, 1.f);
}

struct Arena {
    World world;
    std::vector<Body> crates;
    int crusherIdx = -1;
    float crusherPhase = 0.f;

    void buildBase() {
        world.boxes.clear();
        // границы арены
        auto wall=[&](V3 c, V3 h){
            OBB b; b.c=c; b.h=h; b.fric=0.5f; b.col=Color{95,98,110,255};
            world.boxes.push_back(b);
        };
        wall(V3(0,3,-95), V3(95,3,1));
        wall(V3(0,3, 95), V3(95,3,1));
        wall(V3(-95,3,0), V3(1,3,95));
        wall(V3( 95,3,0), V3(1,3,95));
    }

    void mode(int m) {
        buildBase();
        crates.clear();
        crusherIdx = -1;
        auto box=[&](V3 c,V3 h,Color col,float fr=0.6f){
            OBB b; b.c=c; b.h=h; b.fric=fr; b.col=col;
            world.boxes.push_back(b); return (int)world.boxes.size()-1;
        };
        auto ramp=[&](V3 c,V3 h,float ang){
            OBB b; b.c=c; b.h=h; b.fric=0.9f; b.col=Color{140,130,110,255};
            float cs=std::cos(ang), sn=std::sin(ang);
            b.ax[0]=V3(1,0,0);
            b.ax[1]=V3(0,cs,sn);
            b.ax[2]=V3(0,-sn,cs);
            world.boxes.push_back(b);
        };
        Color wallC{150,150,155,255}, pillarC{170,160,140,255};

        if (m == 1) {                       // КРАШ-ТЕСТ
            box(V3(0,2.5f,40), V3(14,2.5f,1.2f), wallC);
            for (int i=0;i<5;i++)
                box(V3(-8.f+i*4.f, 0.6f, 22), V3(0.5f,0.6f,0.5f), pillarC, 0.4f);
        } else if (m == 2) {                // ТРАМПЛИН
            ramp(V3(0,1.6f,26), V3(6,0.4f,7), -0.30f);
            ramp(V3(-22,2.6f,26), V3(5,0.4f,8), -0.42f);
            for (int i=0;i<6;i++)
                box(V3(-15.f+i*6.f, 1.0f, 62), V3(1.6f,1.0f,1.6f), pillarC, 0.5f);
        } else if (m == 3) {                // ДЕРБИ
            for (int i=0;i<8;i++) {
                float a = i*0.785f;
                box(V3(std::cos(a)*30, 1.2f, std::sin(a)*30), V3(1.4f,1.2f,1.4f), pillarC, 0.5f);
            }
            for (int i=0;i<10;i++) {
                Body b; makeCrate(b, V3(frand(-14,14), 1.0f+i*0.05f, frand(-14,14)),
                                  0.55f, 45.f, Color{170,120,60,255});
                crates.push_back(b);
            }
        } else if (m == 4) {                // ПРЕСС
            OBB b; b.c=V3(0,9,0); b.h=V3(6,1.2f,6);
            b.col=Color{190,70,60,255}; b.fric=0.9f;
            world.boxes.push_back(b);
            crusherIdx = (int)world.boxes.size()-1;
            box(V3(0,0.4f,0), V3(8,0.4f,8), Color{80,80,90,255}, 0.9f);
        } else {                            // ПЕСОЧНИЦА
            ramp(V3(18,1.4f,10), V3(5,0.4f,6), -0.28f);
            box(V3(-20,2.0f,-10), V3(6,2.0f,1.0f), wallC);
            for (int i=0;i<12;i++) {
                Body b; makeCrate(b, V3(-6.f+(i%4)*1.3f, 0.6f+(i/4)*1.2f, 30),
                                  0.55f, 45.f, Color{170,120,60,255});
                crates.push_back(b);
            }
            for (int i=0;i<6;i++)
                box(V3(30.f, 1.5f, -20.f+i*7.f), V3(1.0f,1.5f,1.0f), pillarC, 0.4f);
        }
    }

    void update(float dt) {
        if (crusherIdx >= 0) {
            crusherPhase += dt*0.55f;
            float t = std::fabs(std::sin(crusherPhase));
            OBB& b = world.boxes[crusherIdx];
            float ny = 1.6f + t*8.0f;
            b.vel = V3(0, (ny - b.c.y)/std::max(dt,1e-4f), 0);
            b.c.y = ny;
        }
    }

    void draw() {
        for (auto& b : world.boxes) {
            if (!b.enabled) continue;
            // рисуем через матрицу ориентации
            Matrix m = MatrixIdentity();
            m.m0=b.ax[0].x; m.m1=b.ax[0].y; m.m2 =b.ax[0].z;
            m.m4=b.ax[1].x; m.m5=b.ax[1].y; m.m6 =b.ax[1].z;
            m.m8=b.ax[2].x; m.m9=b.ax[2].y; m.m10=b.ax[2].z;
            m.m12=b.c.x; m.m13=b.c.y; m.m14=b.c.z;
            rlPushMatrix();
            rlMultMatrixf(MatrixToFloat(m));
            DrawCube(V3{0,0,0}, b.h.x*2, b.h.y*2, b.h.z*2, b.col);
            DrawCubeWires(V3{0,0,0}, b.h.x*2, b.h.y*2, b.h.z*2,
                          Color{0,0,0,60});
            rlPopMatrix();
        }
    }
};