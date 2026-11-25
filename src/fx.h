#pragma once
#include "m3.h"
#include "rlgl.h"
#include <vector>

struct Particle {
    V3 p, v; float life, maxLife, size;
    Color c; uint8_t kind;   // 0 искра 1 дым 2 стекло 3 обломок 4 огонь
    float spin, ang;
};

struct FX {
    std::vector<Particle> ps;
    Texture2D soft{};
    int budget = 700;

    struct Skid { V3 a,b; float life; };
    std::vector<Skid> skids;

    void init() {
        Image im = GenImageColor(64,64,BLANK);
        unsigned char* d = (unsigned char*)im.data;
        for (int y=0;y<64;y++) for (int x=0;x<64;x++) {
            float dx=(x-31.5f)/31.5f, dy=(y-31.5f)/31.5f;
            float r=std::sqrt(dx*dx+dy*dy);
            float a=clampf(1.f-r,0.f,1.f); a*=a;
            int i=(y*64+x)*4;
            d[i]=d[i+1]=d[i+2]=255; d[i+3]=(unsigned char)(a*255);
        }
        soft = LoadTextureFromImage(im);
        UnloadImage(im);
        ps.reserve(budget);
    }
    void spawn(V3 p, V3 v, float life, float size, Color c, uint8_t k) {
        if ((int)ps.size() >= budget) return;
        ps.push_back({p,v,life,life,size,c,k,frand(-6,6),0});
    }
    void sparks(V3 p, float power) {
        int n = (int)clampf(power*2.5f, 2, 22);
        for (int i=0;i<n;i++)
            spawn(p, randDir()*frand(2,4+power*0.7f), frand(0.15f,0.5f),
                  frand(0.03f,0.09f), Color{255,(unsigned char)frand(150,230),80,255}, 0);
    }
    void smoke(V3 p, float amt) {
        spawn(p+randDir()*0.15f, V3(frand(-.4f,.4f),frand(0.8f,2.0f),frand(-.4f,.4f)),
              frand(0.8f,1.8f), frand(0.25f,0.5f)*amt, Color{90,90,95,150}, 1);
    }
    void fire(V3 p) {
        spawn(p+randDir()*0.2f, V3(0,frand(1.5f,3.5f),0), frand(0.2f,0.5f),
              frand(0.2f,0.45f), Color{255,(unsigned char)frand(90,170),30,220}, 4);
    }
    void glass(V3 p) {
        for (int i=0;i<10;i++)
            spawn(p, randDir()*frand(1,5), frand(0.6f,1.4f), frand(0.03f,0.07f),
                  Color{170,210,235,220}, 2);
    }
    void debris(V3 p, V3 vel, Color c) {
        for (int i=0;i<4;i++)
            spawn(p, vel*0.6f + randDir()*frand(1,4), frand(1.5f,3.0f),
                  frand(0.07f,0.18f), c, 3);
    }
    void addSkid(V3 a, V3 b) {
        if (skids.size() > 900) skids.erase(skids.begin(), skids.begin()+100);
        skids.push_back({a,b,14.f});
    }

    void update(float dt) {
        for (size_t i=0;i<ps.size();) {
            Particle& p = ps[i];
            p.life -= dt;
            if (p.life <= 0) { ps[i]=ps.back(); ps.pop_back(); continue; }
            float g = (p.kind==1||p.kind==4) ? 2.0f : -18.f;
            p.v.y += g*dt;
            float drag = (p.kind==1) ? 1.6f : 0.35f;
            p.v *= 1.f/(1.f+drag*dt);
            p.p += p.v*dt;
            p.ang += p.spin*dt;
            if (p.p.y < 0.02f && p.kind!=1 && p.kind!=4) {
                p.p.y = 0.02f; p.v.y = -p.v.y*0.32f; p.v.x*=0.7f; p.v.z*=0.7f;
            }
            if (p.kind==1) p.size += dt*0.55f;
            i++;
        }
        for (size_t i=0;i<skids.size();) {
            skids[i].life -= dt;
            if (skids[i].life<=0) { skids[i]=skids.back(); skids.pop_back(); }
            else i++;
        }
    }

    void drawSkids() {
        rlSetTexture(0);
        rlBegin(RL_QUADS);
        for (auto& s : skids) {
            V3 d = norm(s.b - s.a);
            V3 sd = norm(cross(V3(0,1,0), d))*0.11f;
            unsigned char al = (unsigned char)(clampf(s.life/14.f,0,1)*130);
            rlColor4ub(20,20,22,al);
            rlVertex3f(s.a.x-sd.x, 0.015f, s.a.z-sd.z);
            rlVertex3f(s.a.x+sd.x, 0.015f, s.a.z+sd.z);
            rlVertex3f(s.b.x+sd.x, 0.015f, s.b.z+sd.z);
            rlVertex3f(s.b.x-sd.x, 0.015f, s.b.z-sd.z);
        }
        rlEnd();
    }

    void draw(V3 camRight, V3 camUp, bool additive) {
        rlSetTexture(soft.id);
        rlBegin(RL_QUADS);
        for (auto& p : ps) {
            bool add = (p.kind==0 || p.kind==4);
            if (add != additive) continue;
            float t = p.life/p.maxLife;
            unsigned char al = (unsigned char)(p.c.a * (p.kind==1 ? t*0.6f : t));
            V3 r = camRight*p.size, u = camUp*p.size;
            rlColor4ub(p.c.r,p.c.g,p.c.b,al);
            rlTexCoord2f(0,0); rlVertex3f(p.p.x-r.x-u.x, p.p.y-r.y-u.y, p.p.z-r.z-u.z);
            rlTexCoord2f(0,1); rlVertex3f(p.p.x-r.x+u.x, p.p.y-r.y+u.y, p.p.z-r.z+u.z);
            rlTexCoord2f(1,1); rlVertex3f(p.p.x+r.x+u.x, p.p.y+r.y+u.y, p.p.z+r.z+u.z);
            rlTexCoord2f(1,0); rlVertex3f(p.p.x+r.x-u.x, p.p.y+r.y-u.y, p.p.z+r.z-u.z);
        }
        rlEnd();
        rlSetTexture(0);
    }
};

struct ChaseCam {
    Camera3D cam{};
    V3 smoothPos{}, smoothTgt{};
    float dist = 8.5f, height = 3.4f, orbit = 0.f, pitch = 0.18f;
    float shake = 0.f, crashTimer = 0.f;

    void init() {
        cam.up = Vector3{0,1,0}; cam.fovy = 62.f;
        cam.projection = CAMERA_PERSPECTIVE;
    }
    void update(V3 target, V3 fwd, float speed, float dt) {
        if (crashTimer > 0) { crashTimer -= dt; orbit += dt*0.9f; }
        else orbit = smoothTo(orbit, 0.f, 1.5f, dt);
        float c = std::cos(orbit), s = std::sin(orbit);
        V3 back = fwd*-1.f;
        V3 dir = V3(back.x*c + back.z*s, 0, -back.x*s + back.z*c);
        V3 want = target + dir*dist + V3(0, height + pitch*6.f, 0);
        want.y = std::max(want.y, 0.8f);
        smoothPos = lerp(smoothPos, want, 1.f - std::exp(-7.f*dt));
        smoothTgt = lerp(smoothTgt, target + fwd*2.0f + V3(0,0.8f,0),
                         1.f - std::exp(-9.f*dt));
        shake = std::max(0.f, shake - dt*2.2f);
        V3 sh = randDir()*shake*0.22f;
        cam.position = rv(smoothPos + sh);
        cam.target   = rv(smoothTgt + sh*0.4f);
        cam.fovy = 62.f + clampf(std::fabs(speed)*0.30f, 0.f, 16.f);
    }
};