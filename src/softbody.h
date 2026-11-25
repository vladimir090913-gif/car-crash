#pragma once
#include "m3.h"
#include <vector>

namespace sb {

// Ориентированный бокс (стена, трамплин, пресс)
struct OBB {
    V3 c{0,0,0};
    V3 ax[3]{{1,0,0},{0,1,0},{0,0,1}};
    V3 h{1,1,1};
    float fric = 0.6f, rest = 0.05f;
    V3 vel{0,0,0};          // для кинематики (пресс)
    Color col{120,120,130,255};
    bool enabled = true;
};

struct World {
    std::vector<OBB> boxes;
    float gravity = -16.0f;     // аркадно-усиленная гравитация
    float groundFric = 0.8f;

    // Луч вниз: возвращает высоту опоры и нормаль
    bool support(V3 p, float& hy, V3& nrm) const {
        hy = 0.f; nrm = V3(0,1,0);
        bool hit = true;
        for (const auto& b : boxes) {
            if (!b.enabled) continue;
            V3 o = p + V3(0,60,0);
            V3 d = o - b.c;
            V3 lo{dot(d,b.ax[0]), dot(d,b.ax[1]), dot(d,b.ax[2])};
            V3 ld{-dot(b.ax[0],V3(0,1,0)), -dot(b.ax[1],V3(0,1,0)), -dot(b.ax[2],V3(0,1,0))};
            float tmin = 0.f, tmax = 120.f; int axis = -1; float sgn = 1.f;
            bool ok = true;
            for (int i=0;i<3;i++){
                float l = (&lo.x)[i], dd = (&ld.x)[i], hh = (&b.h.x)[i];
                if (std::fabs(dd) < 1e-6f) { if (std::fabs(l) > hh) { ok=false; break; } continue; }
                float t1 = (-hh - l)/dd, t2 = (hh - l)/dd, s = -1.f;
                if (t1 > t2) { std::swap(t1,t2); s = 1.f; }
                if (t1 > tmin) { tmin = t1; axis = i; sgn = s; }
                tmax = std::min(tmax, t2);
                if (tmin > tmax) { ok = false; break; }
            }
            if (ok && axis >= 0 && tmin > 0.f) {
                float y = o.y - tmin;
                if (y > hy) { hy = y; nrm = b.ax[axis]*sgn; }
            }
        }
        return hit;
    }
};

struct Node {
    V3 p, pp, v, p0, force;
    float w = 1.f;              // обратная масса
    float r = 0.11f;            // радиус столкновения
    float fric = 0.5f, rest = 0.02f;
    float damage = 0.f;
    bool contact = false, isWheel = false, grounded = false;
    V3 cn{0,1,0}, surfVel{0,0,0};
    float impact = 0.f;         // сила удара за подшаг (для FX/очков)
};

enum BType : uint8_t { STRUCT, SHEAR, BEND, SUSP, SPRING };

struct Beam {
    int a=0, b=0;
    float L0=1, Lrest=1;        // текущая и исходная длина покоя
    float comp0=1e-7f, comp=1e-7f;
    float damp=250.f;
    float yield=0.022f;         // порог пластичности (деформация)
    float brk=0.42f;            // порог разрыва
    float plastic=0.55f;        // скорость текучести
    float dmgRate=0.22f;
    float damage=0.f;
    bool broken=false;
    uint8_t type=STRUCT;
};

struct Body {
    std::vector<Node> nodes;
    std::vector<Beam> beams;
    V3 bmin, bmax;
    bool isVehicle=false;
    int brokenCount=0;
    float deformEnergy=0.f;

    void aabb() {
        bmin = V3( 1e9f, 1e9f, 1e9f);
        bmax = V3(-1e9f,-1e9f,-1e9f);
        for (auto& n : nodes) {
            bmin.x = std::min(bmin.x, n.p.x); bmax.x = std::max(bmax.x, n.p.x);
            bmin.y = std::min(bmin.y, n.p.y); bmax.y = std::max(bmax.y, n.p.y);
            bmin.z = std::min(bmin.z, n.p.z); bmax.z = std::max(bmax.z, n.p.z);
        }
        bmin -= V3(0.4f,0.4f,0.4f); bmax += V3(0.4f,0.4f,0.4f);
    }
    void translate(V3 d) { for (auto& n : nodes) { n.p += d; n.pp += d; } }
    void setVelocity(V3 v) { for (auto& n : nodes) n.v = v; }
    V3 center() const {
        V3 c{}; float m=0;
        for (auto& n : nodes) { float w = n.w>0 ? 1.f/n.w : 0; c += n.p*w; m += w; }
        return m>0 ? c*(1.f/m) : c;
    }
    V3 velocity() const {
        V3 c{}; float m=0;
        for (auto& n : nodes) { float w = n.w>0 ? 1.f/n.w : 0; c += n.v*w; m += w; }
        return m>0 ? c*(1.f/m) : c;
    }
    void addBeam(int a, int b, float k, float d, uint8_t t,
                 float yld, float brk_, float strength=1.f) {
        Beam bm;
        bm.a=a; bm.b=b;
        bm.Lrest = bm.L0 = len(nodes[b].p - nodes[a].p);
        bm.comp0 = bm.comp = 1.f/(k*strength);
        bm.damp = d; bm.type = t;
        bm.yield = yld; bm.brk = brk_;
        beams.push_back(bm);
    }
};

// ---- столкновение узла с OBB (проекция позиции) ----
inline bool collideOBB(const OBB& b, V3& p, float r, V3& nOut, float& depth) {
    V3 d = p - b.c;
    V3 l{dot(d,b.ax[0]), dot(d,b.ax[1]), dot(d,b.ax[2])};
    bool inside = std::fabs(l.x)<b.h.x && std::fabs(l.y)<b.h.y && std::fabs(l.z)<b.h.z;
    V3 nl; float dep;
    if (inside) {
        float px=b.h.x-std::fabs(l.x), py=b.h.y-std::fabs(l.y), pz=b.h.z-std::fabs(l.z);
        if (px<=py && px<=pz) { nl=V3(signf(l.x),0,0); dep=px+r; }
        else if (py<=pz)      { nl=V3(0,signf(l.y),0); dep=py+r; }
        else                  { nl=V3(0,0,signf(l.z)); dep=pz+r; }
    } else {
        V3 cl{clampf(l.x,-b.h.x,b.h.x), clampf(l.y,-b.h.y,b.h.y), clampf(l.z,-b.h.z,b.h.z)};
        V3 dd = l - cl; float L = len(dd);
        if (L >= r) return false;
        nl = dd*(1.f/std::max(L,1e-6f)); dep = r - L;
    }
    nOut = b.ax[0]*nl.x + b.ax[1]*nl.y + b.ax[2]*nl.z;
    depth = dep;
    p += nOut*dep;
    return true;
}

struct BreakEvent { V3 pos; float power; int type; };

// ---- один подшаг всего мира ----
inline void step(std::vector<Body*>& bodies, World& W, float h,
                 std::vector<BreakEvent>& events) {
    const float invH = 1.f/h;

    // 1. Предсказание
    for (auto* B : bodies) {
        for (auto& n : B->nodes) {
            if (n.w <= 0) continue;
            n.v.y += W.gravity*h;
            n.v += n.force*n.w*h;
            n.force = V3{};
            n.v *= 1.f/(1.f + 0.02f*h);      // сопротивление воздуха
            n.pp = n.p;
            n.p += n.v*h;
            n.contact = false; n.impact = 0.f;
        }
        for (auto& b : B->beams) b.comp = b.comp; // no-op, держим кэш
    }

    // 2. XPBD-решатель балок + замер деформации ДО коррекции
    for (auto* B : bodies) {
        for (auto& bm : B->beams) {
            if (bm.broken) continue;
            Node& A = B->nodes[bm.a];
            Node& Bn = B->nodes[bm.b];
            float ws = A.w + Bn.w; if (ws <= 0) continue;
            V3 d = Bn.p - A.p; float L = len(d); if (L < 1e-7f) continue;
            V3 n = d*(1.f/L);

            // --- пластика и урон по предсказанной длине ---
            float strain = (L - bm.L0)/bm.Lrest;
            float a = std::fabs(strain);
            if (a > bm.yield) {
                float over = a - bm.yield;
                float flow = clampf(over*bm.plastic, 0.f, 0.25f);
                float newL = bm.L0 + (L - bm.L0)*flow;
                bm.L0 = clampf(newL, bm.Lrest*0.30f, bm.Lrest*2.20f);
                bm.damage += over*bm.dmgRate;
                bm.comp = bm.comp0*(1.f + 3.f*std::min(bm.damage, 1.f));
                B->deformEnergy += over*bm.Lrest;
            }
            if (a > bm.brk || bm.damage >= 1.f) {
                bm.broken = true; B->brokenCount++;
                events.push_back({(A.p+Bn.p)*0.5f, a, (int)bm.type});
                continue;
            }

            float C = L - bm.L0;
            float at = bm.comp*invH*invH;
            float dl = (-C - at*0.f)/(ws + at);   // λ сбрасывается каждый подшаг
            A.p  -= n*(dl*A.w);
            Bn.p += n*(dl*Bn.w);
        }
    }

    // 3. Узел-узел между телами
    for (size_t i=0;i<bodies.size();i++) {
        bodies[i]->aabb();
        for (size_t j=i+1;j<bodies.size();j++) {
            Body* A = bodies[i]; Body* Bb = bodies[j];
            if (A->bmax.x<Bb->bmin.x || A->bmin.x>Bb->bmax.x) continue;
            if (A->bmax.y<Bb->bmin.y || A->bmin.y>Bb->bmax.y) continue;
            if (A->bmax.z<Bb->bmin.z || A->bmin.z>Bb->bmax.z) continue;
            for (auto& na : A->nodes) for (auto& nb : Bb->nodes) {
                float rr = na.r + nb.r;
                V3 d = nb.p - na.p; float L2 = len2(d);
                if (L2 >= rr*rr || L2 < 1e-9f) continue;
                float L = std::sqrt(L2); V3 n = d*(1.f/L);
                float ws = na.w + nb.w; if (ws<=0) continue;
                float dep = rr - L;
                na.p -= n*(dep*na.w/ws);
                nb.p += n*(dep*nb.w/ws);
                float imp = dep*invH;
                na.impact = std::max(na.impact, imp);
                nb.impact = std::max(nb.impact, imp);
                na.contact = nb.contact = true;
                na.cn = n*-1.f; nb.cn = n;
                na.fric = nb.fric = 0.5f;
            }
        }
    }

    // 4. Мир
    for (auto* B : bodies) for (auto& n : B->nodes) {
        if (n.w <= 0) continue;
        if (n.p.y - n.r < 0.f) {
            n.p.y = n.r;
            n.contact = true; n.cn = V3(0,1,0);
            n.fric = n.isWheel ? 0.02f : W.groundFric;
            n.rest = 0.02f; n.surfVel = V3{};
        }
        for (auto& b : W.boxes) {
            if (!b.enabled) continue;
            V3 nn; float dep;
            if (collideOBB(b, n.p, n.r, nn, dep)) {
                n.contact = true; n.cn = nn;
                n.fric = n.isWheel ? 0.02f : b.fric;
                n.rest = b.rest; n.surfVel = b.vel;
                n.impact = std::max(n.impact, dep*invH);
            }
        }
    }

    // 5. Скорости из позиций
    for (auto* B : bodies) for (auto& n : B->nodes)
        if (n.w > 0) n.v = (n.p - n.pp)*invH;

    // 6. Демпфирование балок + контактное трение
    for (auto* B : bodies) {
        for (auto& bm : B->beams) {
            if (bm.broken) continue;
            Node& A = B->nodes[bm.a]; Node& Bn = B->nodes[bm.b];
            float ws = A.w + Bn.w; if (ws<=0) continue;
            V3 d = Bn.p - A.p; float L = len(d); if (L<1e-7f) continue;
            V3 n = d*(1.f/L);
            float c = clampf(bm.damp*h*ws, 0.f, 1.f);
            float vn = dot(Bn.v - A.v, n);
            V3 corr = n*(vn*c/ws);
            A.v += corr*A.w; Bn.v -= corr*Bn.w;
        }
        for (auto& n : B->nodes) {
            if (!n.contact || n.w<=0) continue;
            V3 rel = n.v - n.surfVel;
            float vn = dot(rel, n.cn);
            if (vn < 0) {
                n.impact = std::max(n.impact, -vn);
                rel -= n.cn*(vn*(1.f + n.rest));
            }
            V3 vt = rel - n.cn*dot(rel, n.cn);
            rel -= vt*clampf(n.fric*45.f*h, 0.f, 1.f);
            n.v = rel + n.surfVel;
        }
        // урон узла = средний урон его балок
        for (auto& n : B->nodes) n.damage = 0.f;
        std::vector<int> cnt(B->nodes.size(), 0);
        for (auto& bm : B->beams) {
            float d = bm.broken ? 1.f : std::min(bm.damage, 1.f);
            B->nodes[bm.a].damage += d; cnt[bm.a]++;
            B->nodes[bm.b].damage += d; cnt[bm.b]++;
        }
        for (size_t i=0;i<B->nodes.size();i++)
            if (cnt[i]) B->nodes[i].damage /= cnt[i];
    }
}

} // namespace sb