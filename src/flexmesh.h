#pragma once
#include "softbody.h"
#include "rlgl.h"
#include <unordered_map>
#include <cstring>

struct FVert {
    int n[3];
    V3 local;        // координаты в(rest) локальном базисе тройки узлов
    V3 rest;
    Color base;
    V3 pos, nrm;
};
struct FTri { int v[3]; float e[3]; bool dead=false; bool glass=false; };

struct Panel { int a,b,c,d; Color col; bool glass=false; int seg=2; };

struct FlexMesh {
    std::vector<FVert> verts;
    std::vector<FTri>  tris;
    std::vector<unsigned short> idx;
    std::vector<float> vbuf, nbuf;
    std::vector<unsigned char> cbuf;
    Mesh mesh{};
    Material mat{};
    int detachCount = 0;

    static void frame(V3 p0, V3 p1, V3 p2, V3& X, V3& Y, V3& Z) {
        X = norm(p1 - p0);
        Z = norm(cross(X, p2 - p0));
        Y = cross(Z, X);
    }

    int addVert(V3 rest, Color c, const std::vector<Node>& nodes) {
        // дедупликация по позиции
        static thread_local std::unordered_map<long long,int> cache;
        long long key = (long long)(rest.x*200)*4000000LL
                      + (long long)(rest.y*200)*2000LL + (long long)(rest.z*200);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;

        // 3 ближайших невырожденных узла
        int best[3] = {-1,-1,-1}; float bd[3] = {1e9f,1e9f,1e9f};
        for (size_t i=0;i<nodes.size();i++) {
            if (nodes[i].isWheel) continue;
            float d = len2(nodes[i].p0 - rest);
            for (int k=0;k<3;k++) if (d < bd[k]) {
                for (int m=2;m>k;m--) { bd[m]=bd[m-1]; best[m]=best[m-1]; }
                bd[k]=d; best[k]=(int)i; break;
            }
        }
        if (best[2] < 0) best[2] = best[1] >= 0 ? best[1] : 0;
        V3 X,Y,Z;
        frame(nodes[best[0]].p0, nodes[best[1]].p0, nodes[best[2]].p0, X,Y,Z);
        for (int t=0; t<(int)nodes.size() && len(cross(X,Z))<0.1f; t++) {
            best[2] = t;
            frame(nodes[best[0]].p0, nodes[best[1]].p0, nodes[best[2]].p0, X,Y,Z);
        }
        V3 d = rest - nodes[best[0]].p0;
        FVert v; v.n[0]=best[0]; v.n[1]=best[1]; v.n[2]=best[2];
        v.local = V3(dot(d,X), dot(d,Y), dot(d,Z));
        v.rest = rest; v.base = c; v.pos = rest; v.nrm = V3(0,1,0);
        verts.push_back(v);
        int id = (int)verts.size()-1;
        cache[key] = id;
        return id;
    }

    void build(const std::vector<Node>& nodes, const std::vector<Panel>& panels) {
        verts.clear(); tris.clear(); idx.clear();
        std::unordered_map<long long,int> cache;
        auto key = [](V3 p){ return (long long)(p.x*150)*4000000LL
                                  + (long long)(p.y*150)*2000LL + (long long)(p.z*150); };
        auto getV = [&](V3 p, Color c)->int {
            auto it = cache.find(key(p));
            if (it != cache.end()) return it->second;
            int id = addVert(p, c, nodes);
            cache[key(p)] = id; return id;
        };
        for (auto& pn : panels) {
            V3 A=nodes[pn.a].p0, B=nodes[pn.b].p0, C=nodes[pn.c].p0, D=nodes[pn.d].p0;
            int s = pn.seg;
            std::vector<int> g((s+1)*(s+1));
            for (int j=0;j<=s;j++) for (int i=0;i<=s;i++) {
                float u=i/(float)s, v=j/(float)s;
                V3 p = lerp(lerp(A,B,u), lerp(D,C,u), v);
                g[j*(s+1)+i] = getV(p, pn.col);
            }
            for (int j=0;j<s;j++) for (int i=0;i<s;i++) {
                int a=g[j*(s+1)+i], b=g[j*(s+1)+i+1],
                    c=g[(j+1)*(s+1)+i+1], d=g[(j+1)*(s+1)+i];
                auto tri=[&](int x,int y,int z){
                    FTri t; t.v[0]=x; t.v[1]=y; t.v[2]=z; t.glass=pn.glass;
                    for (int e=0;e<3;e++)
                        t.e[e] = len(verts[t.v[(e+1)%3]].rest - verts[t.v[e]].rest);
                    tris.push_back(t);
                };
                tri(a,b,c); tri(a,c,d);
            }
        }
        rebuildIndex();

        int vc = (int)verts.size();
        vbuf.resize(vc*3); nbuf.resize(vc*3); cbuf.resize(vc*4);
        mesh.vertexCount = vc;
        mesh.triangleCount = (int)tris.size();
        mesh.vertices = vbuf.data();
        mesh.normals  = nbuf.data();
        mesh.colors   = cbuf.data();
        mesh.indices  = idx.data();
        UploadMesh(&mesh, true);
        mat = LoadMaterialDefault();
        mat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }

    void rebuildIndex() {
        idx.clear();
        for (auto& t : tris) {
            if (t.dead) { idx.push_back(0); idx.push_back(0); idx.push_back(0); }
            else for (int k=0;k<3;k++) idx.push_back((unsigned short)t.v[k]);
        }
    }

    // возвращает список оторванных треугольников (для дебриса)
    void update(std::vector<Node>& nodes, std::vector<V3>& detached, float dt) {
        detached.clear();
        V3 X,Y,Z;
        for (auto& v : verts) {
            frame(nodes[v.n[0]].p, nodes[v.n[1]].p, nodes[v.n[2]].p, X,Y,Z);
            v.pos = nodes[v.n[0]].p + X*v.local.x + Y*v.local.y + Z*v.local.z;
        }
        // нормали
        for (auto& v : verts) v.nrm = V3{};
        bool dirty = false;
        for (auto& t : tris) {
            if (t.dead) continue;
            V3& a = verts[t.v[0]].pos; V3& b = verts[t.v[1]].pos; V3& c = verts[t.v[2]].pos;
            V3 nn = cross(b-a, c-a);
            // проверка разрыва панели
            float worst = 0;
            for (int e=0;e<3;e++) {
                float L = len(verts[t.v[(e+1)%3]].pos - verts[t.v[e]].pos);
                worst = std::max(worst, std::fabs(L - t.e[e])/std::max(t.e[e],0.01f));
            }
            float dmg = (nodes[verts[t.v[0]].n[0]].damage
                       + nodes[verts[t.v[1]].n[0]].damage
                       + nodes[verts[t.v[2]].n[0]].damage)/3.f;
            if (worst > (t.glass ? 0.22f : 0.62f) || (dmg > 0.85f && worst > 0.25f)) {
                t.dead = true; detachCount++;
                detached.push_back((a+b+c)*(1.f/3.f));
                dirty = true; continue;
            }
            verts[t.v[0]].nrm += nn; verts[t.v[1]].nrm += nn; verts[t.v[2]].nrm += nn;
        }
        if (dirty) {
            rebuildIndex();
            UpdateMeshBuffer(mesh, 7, idx.data(), (int)idx.size()*sizeof(unsigned short), 0);
        }
        const V3 L = norm(V3(0.42f, 0.86f, 0.30f));
        for (size_t i=0;i<verts.size();i++) {
            FVert& v = verts[i];
            v.nrm = norm(v.nrm);
            float nd = std::max(0.f, dot(v.nrm, L));
            float shade = 0.34f + 0.66f*nd;
            float dm = nodes[v.n[0]].damage;
            shade *= 1.f - 0.45f*clampf(dm,0.f,1.f);
            vbuf[i*3+0]=v.pos.x; vbuf[i*3+1]=v.pos.y; vbuf[i*3+2]=v.pos.z;
            nbuf[i*3+0]=v.nrm.x; nbuf[i*3+1]=v.nrm.y; nbuf[i*3+2]=v.nrm.z;
            cbuf[i*4+0]=(unsigned char)clampf(v.base.r*shade,0,255);
            cbuf[i*4+1]=(unsigned char)clampf(v.base.g*shade,0,255);
            cbuf[i*4+2]=(unsigned char)clampf(v.base.b*shade,0,255);
            cbuf[i*4+3]=255;
        }
        UpdateMeshBuffer(mesh, 0, vbuf.data(), (int)vbuf.size()*4, 0);
        UpdateMeshBuffer(mesh, 2, nbuf.data(), (int)nbuf.size()*4, 0);
        UpdateMeshBuffer(mesh, 3, cbuf.data(), (int)cbuf.size(),   0);
    }

    void draw() {
        rlDisableBackfaceCulling();
        DrawMesh(mesh, mat, MatrixIdentity());
        rlEnableBackfaceCulling();
    }
    void unload() { UnloadMesh(mesh); UnloadMaterial(mat); }
};

// генерация панелей кузова из решётки узлов
inline std::vector<Panel> carPanels(const Vehicle& v, Color paint) {
    std::vector<Panel> P;
    Color glass{40,55,75,255}, dark{35,35,40,255};
    const int NS=6;
    int F[NS][3], B[NS][3], R[4][3];
    int idx=0;
    for (int i=0;i<NS;i++) for (int k=0;k<3;k++) { F[i][k]=idx++; B[i][k]=idx++; }
    for (int i=1;i<=3;i++) for (int k=0;k<3;k++) R[i][k]=idx++;
    auto q=[&](int a,int b,int c,int d,Color col,bool g=false,int s=2){
        P.push_back({a,b,c,d,col,g,s});
    };
    // днище
    for (int i=0;i<NS-1;i++) for (int k=0;k<2;k++)
        q(F[i][k],F[i][k+1],F[i+1][k+1],F[i+1][k],dark,false,1);
    // борта
    for (int s=0;s<2;s++) { int k = s?2:0;
        for (int i=0;i<NS-1;i++) q(F[i][k],B[i][k],B[i+1][k],F[i+1][k],paint,false,2);
    }
    // капот / крышка багажника
    for (int i=3;i<NS-1;i++) for (int k=0;k<2;k++)
        q(B[i][k],B[i][k+1],B[i+1][k+1],B[i+1][k],paint,false,2);
    for (int i=0;i<1;i++) for (int k=0;k<2;k++)
        q(B[i][k],B[i][k+1],B[i+1][k+1],B[i+1][k],paint,false,2);
    // крыша
    for (int i=1;i<3;i++) for (int k=0;k<2;k++)
        q(R[i][k],R[i][k+1],R[i+1][k+1],R[i+1][k],paint,false,2);
    // лобовое / заднее
    for (int k=0;k<2;k++) q(R[3][k],R[3][k+1],B[4][k+1],B[4][k],glass,true,2);
    for (int k=0;k<2;k++) q(B[0][k],B[0][k+1],R[1][k+1],R[1][k],glass,true,2);
    // боковины салона + стёкла
    for (int s=0;s<2;s++) { int k=s?2:0;
        for (int i=1;i<3;i++) q(B[i][k],R[i][k],R[i+1][k],B[i+1][k],glass,true,2);
        q(B[3][k],R[3][k],B[4][k],B[4][k],paint,false,1);
        q(B[0][k],B[1][k],R[1][k],R[1][k],paint,false,1);
    }
    // перед / зад
    for (int k=0;k<2;k++) {
        q(F[5][k],F[5][k+1],B[5][k+1],B[5][k],dark,false,1);
        q(F[0][k+1],F[0][k],B[0][k],B[0][k+1],dark,false,1);
    }
    return P;
}