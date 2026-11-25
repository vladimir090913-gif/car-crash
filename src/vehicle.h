#pragma once
#include "softbody.h"
using namespace sb;

struct CarSpec {
    float mass = 1250.f;
    float wheelR = 0.34f, wheelMass = 26.f;
    float springK = 44000.f, springD = 4200.f;
    float tireMu = 1.18f;
    float peakTorque = 360.f, redline = 7200.f, idle = 900.f;
    float gears[6] = {3.55f, 2.28f, 1.62f, 1.24f, 0.98f, 0.79f};
    float diff = 3.42f;
    bool awd = false;
    float strength = 1.f;       // множитель прочности кузова
    Color paint{200,60,50,255};
};

struct Wheel {
    int node = -1, spring = -1;
    std::vector<int> links;
    float omega = 0.f, steer = 0.f, inertia = 1.1f;
    float slipRatio = 0.f, slipAngle = 0.f, load = 0.f;
    bool driven = false, front = false, attached = true, onGround = false;
    V3 contactPoint{};
};

struct Vehicle {
    Body body;
    CarSpec spec;
    Wheel wheels[4];
    int nFront=0, nRear=0, nTop=0, nBottom=0, nLeft=0, nRight=0;
    V3 fwd{0,0,1}, up{0,1,0}, right{1,0,0}, pos{};
    float speed = 0.f, rpm = 900.f, throttle = 0.f, brakeIn = 0.f, steerIn = 0.f;
    int gear = 1;                // 0 = задняя
    float shiftTimer = 0.f, engineDamage = 0.f, totalDamage = 0.f;
    bool handbrake = false, engineOn = true;
    float maxSteer = 0.55f;

    // ---------- построение ----------
    void build(const CarSpec& s, V3 origin, float yaw) {
        spec = s;
        body.isVehicle = true;
        const int NS = 6;
        float zs[NS] = {-2.20f,-1.42f,-0.52f, 0.42f, 1.38f, 2.20f};
        float xs[3]  = {-0.90f, 0.f, 0.90f};
        float yF = 0.34f, yB = 0.88f, yR = 1.42f;
        float beltW[NS] = {0.84f, 0.98f, 1.0f, 1.0f, 0.96f, 0.80f};
        float roofP[4]  = {0.f, 0.94f, 1.0f, 0.90f};

        std::vector<float> mw;
        auto add = [&](V3 p, float m, bool wheel=false, float rad=0.11f) {
            Node n; n.p0 = p; n.p = p; n.pp = p;
            n.w = 1.f/m; n.r = rad; n.isWheel = wheel;
            n.fric = 0.55f;
            body.nodes.push_back(n);
            return (int)body.nodes.size()-1;
        };

        int floorI[NS][3], beltI[NS][3], roofI[4][3];
        for (int i=0;i<NS;i++) for (int k=0;k<3;k++) {
            float wgt = (i>=1 && i<=4) ? 1.5f : 1.0f;
            floorI[i][k] = add(V3(xs[k], yF, zs[i]), wgt);
            beltI[i][k]  = add(V3(xs[k]*beltW[i], yB, zs[i]), 0.75f);
        }
        for (int i=1;i<=3;i++) for (int k=0;k<3;k++)
            roofI[i][k] = add(V3(xs[k]*0.80f, yR*roofP[i], zs[i]), 0.42f);

        // нормировка массы кузова
        float target = s.mass - 4*s.wheelMass, cur = 0;
        for (auto& n : body.nodes) cur += 1.f/n.w;
        float k = target/cur;
        for (auto& n : body.nodes) n.w /= k;

        // балки: все пары, жёсткость падает с длиной
        int N = (int)body.nodes.size();
        for (int a=0;a<N;a++) for (int b=a+1;b<N;b++) {
            float L = len(body.nodes[b].p - body.nodes[a].p);
            float stiff, yld, brk; uint8_t t;
            if      (L < 1.30f) { stiff = 6.0e6f; yld=0.020f; brk=0.40f; t=STRUCT; }
            else if (L < 2.30f) { stiff = 3.2e6f; yld=0.024f; brk=0.46f; t=SHEAR;  }
            else                { stiff = 9.0e5f; yld=0.030f; brk=0.60f; t=BEND;   }
            // зоны деформации: нос/хвост мягче, клетка салона жёстче
            float za = body.nodes[a].p0.z, zb = body.nodes[b].p0.z;
            float zm = (za+zb)*0.5f;
            float zone = (zm > 1.2f || zm < -1.3f) ? 0.55f
                       : (zm > -1.5f && zm < 0.6f) ? 1.75f : 1.0f;
            body.addBeam(a, b, stiff, 260.f, t, yld, brk, zone*s.strength);
        }

        // колёса
        float axZ[2] = {-1.42f, 1.38f};
        int wi = 0;
        for (int f=0; f<2; f++) for (int side=0; side<2; side++) {
            float sx = side ? 1.f : -1.f;
            V3 wp(sx*0.88f, s.wheelR, axZ[f]);
            int idx = add(wp, s.wheelMass, true, 0.20f);
            Wheel& w = wheels[wi];
            w.node = idx; w.front = (f==1);
            w.driven = s.awd || (f==0);
            w.inertia = 1.2f;
            // рычаги: почти горизонтальные жёсткие балки к нижним узлам
            for (int a=0;a<N;a++) {
                V3 d = body.nodes[a].p0 - wp;
                if (std::fabs(d.y) < 0.10f && std::sqrt(d.x*d.x+d.z*d.z) < 1.45f) {
                    body.addBeam(idx, a, 9.0e6f, 900.f, SUSP, 0.05f, 0.55f, s.strength);
                    w.links.push_back((int)body.beams.size()-1);
                }
            }
            // пружина к верхнему узлу той же станции
            int st = (f==1) ? 4 : 1;
            int top = beltI[st][side?2:0];
            body.addBeam(idx, top, s.springK, s.springD, SPRING, 0.40f, 1.10f, 1.f);
            w.spring = (int)body.beams.size()-1;
            wi++;
        }

        nFront = floorI[5][1]; nRear = floorI[0][1];
        nLeft  = floorI[2][0]; nRight = floorI[2][2];
        nTop   = roofI[2][1];  nBottom = floorI[2][1];

        // поворот + перенос
        float c = std::cos(yaw), sn = std::sin(yaw);
        for (auto& n : body.nodes) {
            V3 p = n.p0;
            V3 r(p.x*c + p.z*sn, p.y, -p.x*sn + p.z*c);
            n.p0 = r; n.p = n.pp = r + origin;
        }
        for (auto& bm : body.beams) bm.Lrest = bm.L0 = len(body.nodes[bm.b].p - body.nodes[bm.a].p);
        updateFrame();
    }

    void updateFrame() {
        auto& N = body.nodes;
        fwd   = norm(N[nFront].p - N[nRear].p);
        V3 u0 = norm(N[nTop].p - N[nBottom].p);
        right = norm(cross(u0, fwd));
        up    = norm(cross(fwd, right));
        pos   = body.center();
        speed = dot(body.velocity(), fwd);
    }

    static float magic(float x, float B, float C, float E) {
        float bx = B*x;
        return std::sin(C*std::atan(bx - E*(bx - std::atan(bx))));
    }
    float torqueCurve(float r) const {
        float t = clampf((r - 800.f)/6200.f, 0.f, 1.2f);
        return spec.peakTorque*(0.45f + 0.85f*t - 0.42f*t*t);
    }

    // ---------- один подшаг ----------
    void control(float h, const World& W) {
        updateFrame();
        float absSpd = std::fabs(speed);

        // руль с учётом скорости
        float ms = maxSteer*(0.28f + 0.72f/(1.f + absSpd*0.09f));
        float tgt = steerIn*ms;
        float rate = 5.5f*h;
        for (int i=0;i<4;i++) if (wheels[i].front)
            wheels[i].steer += clampf(tgt - wheels[i].steer, -rate, rate);

        // КПП
        shiftTimer -= h;
        int drivenCnt = 0; float wsum = 0;
        for (auto& w : wheels) if (w.driven && w.attached) { wsum += std::fabs(w.omega); drivenCnt++; }
        float wavg = drivenCnt ? wsum/drivenCnt : 0;
        float gr = gear == 0 ? 3.4f : spec.gears[gear-1];
        rpm = clampf(wavg*gr*spec.diff*9.5493f, spec.idle, spec.redline);
        if (shiftTimer <= 0 && gear > 0) {
            if (rpm > spec.redline*0.90f && gear < 6) { gear++; shiftTimer = 0.35f; }
            else if (rpm < spec.redline*0.33f && gear > 1) { gear--; shiftTimer = 0.30f; }
        }
        // задняя передача
        if (brakeIn > 0.2f && speed < 1.0f) gear = 0;
        else if (gear == 0 && throttle > 0.1f && speed > -0.5f) gear = 1;

        float thr = (gear == 0) ? brakeIn : throttle;
        float brk = (gear == 0) ? throttle : brakeIn;
        float health = engineOn ? (1.f - clampf(engineDamage,0.f,1.f)*0.9f) : 0.f;
        float clutch = clampf((rpm - spec.idle*0.6f)/1200.f, 0.25f, 1.f);
        float engT = torqueCurve(rpm)*thr*health*clutch;
        if (rpm >= spec.redline*0.99f) engT *= 0.15f;
        float driveT = engT*gr*spec.diff*0.90f/std::max(drivenCnt,1);
        if (gear == 0) driveT = -driveT;

        gr = gear == 0 ? 3.4f : spec.gears[gear-1];

        for (int i=0;i<4;i++) {
            Wheel& w = wheels[i];
            Node& n = body.nodes[w.node];

            // отрыв колеса
            if (w.attached) {
                int alive = 0;
                for (int bi : w.links) if (!body.beams[bi].broken) alive++;
                if (alive == 0 && body.beams[w.spring].broken) {
                    w.attached = false; n.r = spec.wheelR; n.fric = 0.9f;
                }
            }
            if (!w.attached) { w.omega *= 0.995f; continue; }

            V3 f = norm(fwd*std::cos(w.steer) + right*std::sin(w.steer));
            V3 sd = norm(cross(up, f));

            float hy; V3 gn;
            W.support(n.p, hy, gn);
            float dist = n.p.y - hy;
            float pen = spec.wheelR - dist;
            w.onGround = pen > 0 && gn.y > 0.3f;
            w.contactPoint = V3(n.p.x, hy, n.p.z);

            if (w.onGround) {
                float penV = -dot(n.v, gn);
                float Fz = std::max(0.f, pen*190000.f + penV*5200.f);
                Fz = std::min(Fz, 45000.f);
                w.load = Fz;
                n.force += gn*Fz;

                float vLong = dot(n.v, f);
                float vLat  = dot(n.v, sd);
                float denom = std::max(std::fabs(vLong), 2.0f);
                w.slipRatio = (w.omega*spec.wheelR - vLong)/denom;
                w.slipAngle = std::atan2(vLat, std::max(std::fabs(vLong), 1.0f));

                float mu = spec.tireMu;
                float Fx =  Fz*mu*magic(w.slipRatio, 10.f, 1.9f, 0.97f);
                float Fy = -Fz*mu*magic(w.slipAngle,  8.f, 1.35f, -1.0f);
                // эллипс трения
                float comb = std::sqrt((Fx/(mu*Fz+1))*(Fx/(mu*Fz+1)) + (Fy/(mu*Fz+1))*(Fy/(mu*Fz+1)));
                if (comb > 1.f) { Fx /= comb; Fy /= comb; }
                n.force += f*Fx + sd*Fy;

                float bt = brk*2600.f + ((handbrake && !w.front) ? 5200.f : 0.f);
                float T = (w.driven ? driveT : 0.f) - Fx*spec.wheelR;
                w.omega += T/w.inertia*h;
                // тормоз
                float dw = bt/w.inertia*h;
                if (std::fabs(w.omega) <= dw) w.omega = 0.f;
                else w.omega -= signf(w.omega)*dw;
            } else {
                w.load = 0; w.slipRatio = 0; w.slipAngle = 0;
                float T = (w.driven ? driveT : 0.f) - brk*2600.f*signf(w.omega);
                w.omega += T/w.inertia*h;
                w.omega *= 1.f/(1.f + 0.4f*h);
            }
            w.omega = clampf(w.omega, -400.f, 400.f);
        }

        // агрегированный урон
        float dmg = 0, front = 0; int fc = 0;
        for (auto& bm : body.beams) {
            float d = bm.broken ? 1.f : std::min(bm.damage, 1.f);
            dmg += d;
            float zm = (body.nodes[bm.a].p0.z + body.nodes[bm.b].p0.z)*0.5f;
            if (zm > 1.0f) { front += d; fc++; }
        }
        totalDamage = dmg/std::max((size_t)1, body.beams.size());
        engineDamage = fc ? front/fc : 0;
        if (engineDamage > 0.92f) engineOn = false;
    }

    void reset(V3 origin, float yaw) {
        CarSpec s = spec;
        body = Body{};
        *this = Vehicle{};
        build(s, origin, yaw);
    }
};