#pragma once
#include "m3.h"
#include <vector>
#include <cstring>

enum WId { W_NONE=-1, W_STEER, W_GAS, W_BRAKE, W_HAND,
           W_RESET, W_CAM, W_SLOW, W_MODE, W_SPAWN, W_COUNT };

struct Input {
    float steer=0, gas=0, brake=0;
    bool hand=false;
    bool reset=false, cam=false, slow=false, mode=false, spawn=false;
    float camDX=0, camDY=0, zoom=0;
};

struct TouchUI {
    Rectangle r[W_COUNT]{};
    V3 steerCenter{};
    float steerRadius = 100.f, S = 1.f;
    int owner[10];                 // touch id -> widget
    int ids[10];
    float steerVisual = 0.f;
    bool showHelp = true;

    TouchUI() { for (int i=0;i<10;i++){ owner[i]=W_NONE; ids[i]=-1; } }

    void layout(int W, int H) {
        S = H/720.f;
        float s = S;
        steerCenter = V3(160*s, H-160*s, 0);
        steerRadius = 118*s;
        r[W_STEER] = Rectangle{steerCenter.x-steerRadius, steerCenter.y-steerRadius,
                               steerRadius*2, steerRadius*2};
        r[W_GAS]   = Rectangle{W-160*s, H-280*s, 130*s, 240*s};
        r[W_BRAKE] = Rectangle{W-320*s, H-215*s, 130*s, 175*s};
        r[W_HAND]  = Rectangle{W-450*s, H-140*s, 105*s, 100*s};
        float bw = 92*s, bh = 56*s, gap = 8*s;
        float x = W - (bw+gap)*5 - 10*s;
        for (int i=0;i<5;i++)
            r[W_RESET+i] = Rectangle{x + i*(bw+gap), 12*s, bw, bh};
    }

    int hit(V3 p) const {
        for (int i=0;i<W_COUNT;i++)
            if (CheckCollisionPointRec(Vector2{p.x,p.y}, r[i])) return i;
        return W_NONE;
    }
    int slot(int id) {
        for (int i=0;i<10;i++) if (ids[i]==id) return i;
        for (int i=0;i<10;i++) if (ids[i]==-1) { ids[i]=id; owner[i]=W_NONE; return i; }
        return -1;
    }

    void update(Input& in, int W, int H) {
        in.reset=in.cam=in.slow=in.mode=in.spawn=false;
        in.gas=in.brake=0; in.hand=false;
        in.camDX=in.camDY=in.zoom=0;
        float steerTarget = 0;
        bool steerHeld = false;

        int n = GetTouchPointCount();
        bool used[10] = {false};
        for (int t=0;t<n && t<10;t++) {
            int id = GetTouchPointId(t);
            Vector2 tp = GetTouchPosition(t);
            V3 p(tp.x, tp.y, 0);
            int sl = slot(id); if (sl<0) continue;
            used[sl]=true;
            if (owner[sl]==W_NONE) { owner[sl]=hit(p); if (owner[sl]==W_NONE) owner[sl]=-2; }
            int w = owner[sl];
            switch (w) {
                case W_STEER: steerTarget = clampf((p.x-steerCenter.x)/steerRadius,-1,1);
                              steerHeld=true; break;
                case W_GAS:   in.gas = 1; break;
                case W_BRAKE: in.brake = 1; break;
                case W_HAND:  in.hand = true; break;
                case W_RESET: in.reset=true; break;
                case W_CAM:   in.cam=true; break;
                case W_SLOW:  in.slow=true; break;
                case W_MODE:  in.mode=true; break;
                case W_SPAWN: in.spawn=true; break;
                case -2: {
                    static Vector2 last[10];
                    in.camDX += (tp.x - last[sl].x)*0.006f;
                    in.camDY += (tp.y - last[sl].y)*0.004f;
                    last[sl] = tp;
                } break;
            }
        }
        // мышь для десктопа
        if (n == 0) {
            static int mouseOwner = W_NONE;
            static Vector2 lastM{};
            Vector2 mp = GetMousePosition();
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) mouseOwner = hit(V3(mp.x,mp.y,0));
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) mouseOwner = W_NONE;
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                V3 p(mp.x,mp.y,0);
                switch (mouseOwner) {
                    case W_STEER: steerTarget=clampf((p.x-steerCenter.x)/steerRadius,-1,1);
                                  steerHeld=true; break;
                    case W_GAS: in.gas=1; break;
                    case W_BRAKE: in.brake=1; break;
                    case W_HAND: in.hand=true; break;
                    case W_RESET: in.reset=true; break;
                    case W_CAM: in.cam=true; break;
                    case W_SLOW: in.slow=true; break;
                    case W_MODE: in.mode=true; break;
                    case W_SPAWN: in.spawn=true; break;
                    default:
                        in.camDX += (mp.x-lastM.x)*0.006f;
                        in.camDY += (mp.y-lastM.y)*0.004f;
                }
            }
            lastM = mp;
            in.zoom = -GetMouseWheelMove()*0.6f;
        }
        for (int i=0;i<10;i++) if (!used[i] && ids[i]!=-1) { ids[i]=-1; owner[i]=W_NONE; }

        // клавиатура
        if (IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT))  { steerTarget=-1; steerHeld=true; }
        if (IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT)) { steerTarget= 1; steerHeld=true; }
        if (IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))   in.gas=1;
        if (IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN)) in.brake=1;
        if (IsKeyDown(KEY_SPACE)) in.hand=true;
        if (IsKeyPressed(KEY_R)) in.reset=true;
        if (IsKeyPressed(KEY_C)) in.cam=true;
        if (IsKeyPressed(KEY_T)) in.slow=true;
        if (IsKeyPressed(KEY_M)) in.mode=true;
        if (IsKeyPressed(KEY_F)) in.spawn=true;

        in.steer = steerHeld ? steerTarget
                             : smoothTo(in.steer, 0.f, 9.f, GetFrameTime());
        steerVisual = smoothTo(steerVisual, in.steer, 18.f, GetFrameTime());
    }

    void draw(int W, int H, const Input& in, bool slowmo, int mode, float dmg) {
        float s = S;
        auto btn=[&](int id,const char* txt,bool active){
            Rectangle q=r[id];
            DrawRectangleRounded(q, 0.3f, 6, active ? Color{70,150,220,220}
                                                    : Color{20,22,30,170});
            DrawRectangleRoundedLines(q, 0.3f, 6, Color{255,255,255,60});
            int tw = MeasureText(txt, (int)(18*s));
            DrawText(txt, (int)(q.x+q.width/2-tw/2), (int)(q.y+q.height/2-9*s),
                     (int)(18*s), RAYWHITE);
        };
        // джойстик
        DrawCircle((int)steerCenter.x,(int)steerCenter.y, steerRadius, Color{20,22,30,150});
        DrawCircleLines((int)steerCenter.x,(int)steerCenter.y, steerRadius, Color{255,255,255,70});
        DrawCircle((int)(steerCenter.x+steerVisual*steerRadius*0.78f),
                   (int)steerCenter.y, 44*s, Color{90,170,240,220});
        DrawText("STEER", (int)(steerCenter.x-28*s), (int)(steerCenter.y-steerRadius-24*s),
                 (int)(16*s), Color{255,255,255,140});
        // педали
        auto pedal=[&](int id,const char* t,float v,Color c){
            Rectangle q=r[id];
            DrawRectangleRounded(q,0.22f,6,Color{20,22,30,170});
            Rectangle f=q; f.height=q.height*v; f.y=q.y+q.height*(1-v);
            DrawRectangleRounded(f,0.22f,6,c);
            DrawRectangleRoundedLines(q,0.22f,6,Color{255,255,255,60});
            int tw=MeasureText(t,(int)(18*s));
            DrawText(t,(int)(q.x+q.width/2-tw/2),(int)(q.y+10*s),(int)(18*s),RAYWHITE);
        };
        pedal(W_GAS,"GAS",in.gas,Color{60,190,90,220});
        pedal(W_BRAKE,"BRK",in.brake,Color{220,70,60,220});
        btn(W_HAND,"P-BRK",in.hand);
        btn(W_RESET,"RESET",false);
        btn(W_CAM,"CAM",false);
        btn(W_SLOW,"SLOW",slowmo);
        btn(W_MODE,"MODE",false);
        btn(W_SPAWN,"CAR",false);

        const char* names[]={"SANDBOX","CRASH TEST","RAMP JUMP","DERBY","CRUSHER"};
        DrawText(names[mode], (int)(14*s), (int)(14*s), (int)(24*s), RAYWHITE);
        //.damage bar
        DrawRectangle((int)(14*s),(int)(46*s),(int)(200*s),(int)(14*s),Color{0,0,0,140});
        DrawRectangle((int)(14*s),(int)(46*s),(int)(200*s*clampf(dmg,0,1)),(int)(14*s),
                      Color{230,90,50,230});
        DrawText("DAMAGE",(int)(14*s),(int)(64*s),(int)(14*s),Color{255,255,255,170});
    }
};