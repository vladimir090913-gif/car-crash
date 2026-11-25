#include "raylib.h"
#include "rlgl.h"
#include "vehicle.h"
#include "flexmesh.h"
#include "fx.h"
#include "arena.h"
#include "touch.h"
#include "audio.h"
#include <vector>
#include <memory>

struct CarEntity {
    Vehicle veh;
    FlexMesh mesh;
    bool ai = false;
    V3 lastSkid[4]{};
    bool skidInit[4]{false,false,false,false};
};

struct Game {
    Arena arena;
    std::vector<std::unique_ptr<CarEntity>> cars;
    std::vector<Body> crateBodies;
    std::vector<FlexMesh> crateMeshes;
    FX fx;
    ChaseCam cam;
    TouchUI ui;
    GameAudio audio;
    Input in;
    Texture2D ground{};
    Mesh groundMesh{};
    Material groundMat{};

    int mode = 0, camMode = 0;
    bool slowmo = false;
    float timeScale = 1.f, score = 0, bestScore = 0;
    float airTime = 0, jumpStart = 0, messageTimer = 0;
    char message[96] = "";
    float spawnYaw = 0.f;

    void say(const char* t) { snprintf(message, sizeof(message), "%s", t); messageTimer = 2.5f; }

    void init() {
        fx.init(); cam.init(); audio.init();
        makeGround();
        setMode(0);
    }

    void makeGround() {
        Image im = GenImageColor(256,256,Color{58,58,62,255});
        unsigned char* d=(unsigned char*)im.data;
        for (int y=0;y<256;y++) for (int x=0;x<256;x++) {
            int i=(y*256+x)*4;
            int n=(rand()%18)-9;
            bool line = (x<3||y<3);
            d[i]  = clampf((line?78:58)+n,0,255);
            d[i+1]= clampf((line?78:58)+n,0,255);
            d[i+2]= clampf((line?84:62)+n,0,255);
            d[i+3]=255;
        }
        ground = LoadTextureFromImage(im);
        UnloadImage(im);
        SetTextureWrap(ground, TEXTURE_WRAP_REPEAT);

        static float v[12] = {-100,0,-100, 100,0,-100, 100,0,100, -100,0,100};
        static float uv[8] = {0,0, 50,0, 50,50, 0,50};
        static float nr[12]= {0,1,0, 0,1,0, 0,1,0, 0,1,0};
        static unsigned short ix[6]={0,1,2, 0,2,3};
        groundMesh.vertexCount=4; groundMesh.triangleCount=2;
        groundMesh.vertices=v; groundMesh.texcoords=uv;
        groundMesh.normals=nr; groundMesh.indices=ix;
        UploadMesh(&groundMesh,false);
        groundMat = LoadMaterialDefault();
        groundMat.maps[MATERIAL_MAP_DIFFUSE].texture = ground;
    }

    CarEntity* spawnCar(V3 pos, float yaw, Color paint, bool ai=false, float strength=1.f) {
        auto c = std::make_unique<CarEntity>();
        CarSpec s; s.paint = paint; s.strength = strength;
        if (ai) { s.mass = 1450.f; s.peakTorque = 420.f; s.strength = 1.3f; }
        c->veh.build(s, pos, yaw);
        c->mesh.build(c->veh.body.nodes, carPanels(c->veh, paint));
        c->ai = ai;
        cars.push_back(std::move(c));
        return cars.back().get();
    }

    void setMode(int m) {
        mode = m;
        arena.mode(m);
        cars.clear();
        crateBodies = arena.crates;
        crateMeshes.clear();
        for (auto& b : crateBodies) {
            FlexMesh fm;
            std::vector<Panel> P;
            Color c{170,120,60,255};
            int q[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
            for (auto& f : q) P.push_back({f[0],f[1],f[2],f[3],c,false,1});
            fm.build(b.nodes, P);
            crateMeshes.push_back(std::move(fm));
        }
        score = 0; airTime = 0;
        V3 start(0,0.2f,-20);
        if (m==1) start = V3(0,0.2f,-40);
        if (m==4) start = V3(0,0.9f,0);
        spawnCar(start, 0.f, Color{210,60,50,255});
        if (m==3) {
            spawnCar(V3(18,0.2f,10), 3.14f, Color{50,110,200,255}, true);
            spawnCar(V3(-18,0.2f,10), 3.14f, Color{60,170,90,255}, true);
        }
        cam.smoothPos = start + V3(0,4,-10);
        cam.smoothTgt = start;
        const char* n[]={"SANDBOX","CRASH TEST","RAMP JUMP","DERBY","CRUSHER"};
        say(n[m]);
    }

    void input(int W, int H) {
        ui.layout(W,H);
        ui.update(in, W, H);
        if (in.reset && !cars.empty()) {
            V3 p = cars[0]->veh.pos; p.y = 1.2f;
            cars[0]->veh.reset(V3(p.x,0.3f,p.z), 0.f);
            cars[0]->mesh.unload();
            cars[0]->mesh.build(cars[0]->veh.body.nodes,
                                carPanels(cars[0]->veh, cars[0]->veh.spec.paint));
            say("RESET");
        }
        if (in.cam) { camMode = (camMode+1)%3; say("CAMERA"); }
        if (in.slow) { slowmo = !slowmo; say(slowmo?"SLOW MOTION ON":"SLOW MOTION OFF"); }
        if (in.mode) setMode((mode+1)%5);
        if (in.spawn && cars.size() < 5)
            spawnCar(cars[0]->veh.pos + V3(frand(-6,6), 3.f, frand(-6,6)),
                     frand(0,6.28f), Color{(unsigned char)frand(60,240),
                                           (unsigned char)frand(60,240),
                                           (unsigned char)frand(60,240),255});
        cam.orbit -= in.camDX;
        cam.pitch = clampf(cam.pitch + in.camDY, -0.2f, 0.9f);
        cam.dist  = clampf(cam.dist + in.zoom, 4.f, 22.f);
    }

    void update(float rawDt) {
        float dt = rawDt*(slowmo ? 0.22f : 1.f);
        timeScale = slowmo ? 0.22f : 1.f;
        arena.update(dt);

        // ИИ дерби
        for (size_t i=1;i<cars.size();i++) {
            auto* c = cars[i].get();
            if (!c->ai) continue;
            V3 d = cars[0]->veh.pos - c->veh.pos;
            float want = std::atan2(dot(d, c->veh.right), dot(d, c->veh.fwd));
            c->veh.steerIn = clampf(want*1.8f, -1.f, 1.f);
            c->veh.throttle = 1.f;
            c->veh.brakeIn = 0.f;
        }
        if (!cars.empty()) {
            cars[0]->veh.steerIn  = in.steer;
            cars[0]->veh.throttle = in.gas;
            cars[0]->veh.brakeIn  = in.brake;
            cars[0]->veh.handbrake= in.hand;
        }

        // физика
        std::vector<Body*> bodies;
        for (auto& c : cars) bodies.push_back(&c->veh.body);
        for (auto& b : crateBodies) bodies.push_back(&b);

        const float H = 1.f/600.f;
        int steps = (int)clampf(std::round(dt/H), 1.f, 24.f);
        float h = dt/steps;
        std::vector<BreakEvent> events;
        for (int s=0;s<steps;s++) {
            for (auto& c : cars) c->veh.control(h, arena.world);
            sb::step(bodies, arena.world, h, events);
        }

        // события разрушения
        float maxImpact = 0;
        for (auto& e : events) {
            if (e.power > 0.25f) fx.sparks(e.pos, e.power*6.f);
            score += e.power*12.f;
        }
        for (auto& c : cars) {
            for (auto& n : c->veh.body.nodes) {
                if (n.impact > 4.f && !n.isWheel) {
                    maxImpact = std::max(maxImpact, n.impact);
                    if (frand(0,1) < 0.25f) fx.sparks(n.p, n.impact*0.6f);
                    score += n.impact*0.6f;
                }
            }
            // дым/огонь
            V3 eng = c->veh.pos + c->veh.fwd*1.5f + c->veh.up*0.4f;
            if (c->veh.engineDamage > 0.45f && frand(0,1) < c->veh.engineDamage*0.6f)
                fx.smoke(eng, 1.f + c->veh.engineDamage);
            if (c->veh.engineDamage > 0.85f && frand(0,1) < 0.5f) fx.fire(eng);

            // визг + следы
            for (int i=0;i<4;i++) {
                Wheel& w = c->veh.wheels[i];
                if (!w.attached) { c->skidInit[i]=false; continue; }
                float slip = std::fabs(w.slipRatio)*0.8f + std::fabs(w.slipAngle)*1.6f;
                bool skidding = w.onGround && slip > 0.32f && w.load > 500.f;
                if (skidding) {
                    if (c->skidInit[i]) fx.addSkid(c->lastSkid[i], w.contactPoint);
                    c->lastSkid[i] = w.contactPoint; c->skidInit[i]=true;
                } else c->skidInit[i]=false;
            }
        }
        if (maxImpact > 6.f) {
            audio.bang(maxImpact);
            cam.shake = std::min(1.4f, maxImpact*0.07f);
            if (maxImpact > 14.f && cam.crashTimer <= 0) {
                cam.crashTimer = 1.1f;
                if (!slowmo) { slowmo = true; messageTimer = 1.2f; say("BIG CRASH!"); }
            }
        }

        // сетки
        std::vector<V3> detached;
        for (auto& c : cars) {
            c->mesh.update(c->veh.body.nodes, detached, dt);
            for (auto& p : detached) {
                fx.debris(p, c->veh.body.velocity(), c->veh.spec.paint);
                fx.glass(p);
                score += 40;
            }
        }
        for (size_t i=0;i<crateBodies.size();i++) {
            crateMeshes[i].update(crateBodies[i].nodes, detached, dt);
            for (auto& p : detached) fx.debris(p, crateBodies[i].velocity(),
                                              Color{170,120,60,255});
        }

        // трамплин: время в воздухе
        if (!cars.empty()) {
            bool air = true;
            for (auto& w : cars[0]->veh.wheels) if (w.onGround) air = false;
            if (air) airTime += dt;
            else if (airTime > 0.6f) {
                score += airTime*250;
                say(TextFormat("AIR %.2fs  +%d", airTime, (int)(airTime*250)));
                airTime = 0;
            } else airTime = 0;
        }

        fx.update(dt);
        if (!cars.empty()) {
            auto& v = cars[0]->veh;
            audio.updateEngine(v.rpm, v.throttle, v.spec.redline);
            float scr = 0;
            for (auto& w : v.wheels)
                if (w.onGround && w.attached)
                    scr = std::max(scr, std::fabs(w.slipRatio)*0.7f + std::fabs(w.slipAngle)*1.4f);
            audio.updateScreech(scr - 0.3f);

            V3 cp = v.pos;
            if (camMode==1) cp = v.pos + v.up*0.55f + v.fwd*0.3f;   // капот
            if (camMode==2) { cam.dist = 16; cam.pitch = 0.7f; }
            cam.update(cp, v.fwd, v.speed, rawDt);
        }
        bestScore = std::max(bestScore, score);
        messageTimer -= rawDt;
    }

    void draw(int W, int H) {
        BeginDrawing();
        ClearBackground(Color{110,145,185,255});
        BeginMode3D(cam.cam);
            DrawMesh(groundMesh, groundMat, MatrixIdentity());
            arena.draw();
            fx.drawSkids();
            for (auto& m : crateMeshes) m.draw();
            for (auto& c : cars) {
                c->mesh.draw();
                // колёса
                for (int i=0;i<4;i++) {
                    Wheel& w = c->veh.wheels[i];
                    Node& n = c->veh.body.nodes[w.node];
                    V3 axis = c->veh.right;
                    if (w.front) axis = norm(c->veh.right*std::cos(w.steer)
                                           + c->veh.fwd*std::sin(w.steer));
                    V3 a = n.p - axis*0.13f, b = n.p + axis*0.13f;
                    DrawCylinderEx(rv(a), rv(b), c->veh.spec.wheelR,
                                   c->veh.spec.wheelR, 12, Color{25,25,28,255});
                    DrawCylinderEx(rv(a-axis*0.01f), rv(b+axis*0.01f),
                                   c->veh.spec.wheelR*0.55f, c->veh.spec.wheelR*0.55f,
                                   10, Color{150,150,158,255});
                }
                // тень-блоб
                DrawPlane(Vector3{c->veh.pos.x, 0.02f, c->veh.pos.z},
                          Vector2{4.6f, 5.6f}, Color{0,0,0,60});
            }
            V3 cr = norm(cross(vv(cam.target)-vv(cam.position), V3(0,1,0)));
            V3 cu = norm(cross(cr, norm(vv(cam.target)-vv(cam.position))))*-1.f;
            BeginBlendMode(BLEND_ALPHA);
            fx.draw(cr, cu, false);
            BeginBlendMode(BLEND_ADDITIVE);
            fx.draw(cr, cu, true);
            BeginBlendMode(BLEND_ALPHA);
        EndMode3D();

        ui.draw(W,H,in,slowmo,mode, cars.empty()?0:cars[0]->veh.totalDamage);
        float s = ui.S;
        if (!cars.empty()) {
            int kmh = (int)(std::fabs(cars[0]->veh.speed)*3.6f);
            const char* t = TextFormat("%d km/h", kmh);
            DrawText(t, (int)(W/2-MeasureText(t,(int)(40*s))/2), (int)(H-70*s),
                     (int)(40*s), RAYWHITE);
            DrawText(TextFormat("GEAR %s%d  RPM %d",
                     cars[0]->veh.gear==0?"R":"", cars[0]->veh.gear==0?1:cars[0]->veh.gear,
                     (int)cars[0]->veh.rpm),
                     (int)(W/2-70*s), (int)(H-26*s), (int)(18*s), Color{255,255,255,180});
        }
        DrawText(TextFormat("SCORE %d   BEST %d", (int)score, (int)bestScore),
                 (int)(14*s), (int)(86*s), (int)(20*s), Color{255,225,120,230});
        if (messageTimer > 0) {
            int tw = MeasureText(message, (int)(30*s));
            DrawText(message, W/2-tw/2, (int)(H*0.22f), (int)(30*s),
                     Color{255,255,255,(unsigned char)clampf(messageTimer*180,0,255)});
        }
        DrawText(TextFormat("%d FPS", GetFPS()), W-90, H-22, 16, Color{255,255,255,120});
        EndDrawing();
    }

    void shutdown() {
        for (auto& c : cars) c->mesh.unload();
        for (auto& m : crateMeshes) m.unload();
        UnloadMesh(groundMesh); UnloadMaterial(groundMat); UnloadTexture(ground);
        UnloadTexture(fx.soft);
        audio.shutdown();
    }
};

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "CrashForge");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();

    Game g;
    g.init();
    while (!WindowShouldClose()) {
        int W = GetScreenWidth(), H = GetScreenHeight();
        float dt = std::min(GetFrameTime(), 1.f/30.f);
        g.input(W,H);
        g.update(dt);
        g.draw(W,H);
    }
    g.shutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}