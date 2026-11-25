#pragma once
#include "raylib.h"
#include <cmath>
#include <vector>

struct GameAudio {
    Sound engine{}, crash{}, screech{};
    bool ok = false;
    float engVol = 0;

    void init() {
        const int SR = 22050;
        // движок: сумма гармоник с "рычанием"
        int N = SR/40;                     // один период 40 Гц
        std::vector<short> buf(N);
        for (int i=0;i<N;i++) {
            float t = i/(float)N;
            float v = 0;
            for (int h=1; h<=8; h++)
                v += std::sin(2*PI*h*t + h*0.7f)/h * (h%2 ? 1.f : 0.6f);
            v += 0.25f*std::sin(2*PI*0.5f*t);
            buf[i] = (short)(clampf(v*0.30f,-1,1)*30000);
        }
        Wave w{}; w.frameCount=N; w.sampleRate=SR; w.sampleSize=16; w.channels=1;
        w.data = buf.data();
        engine = LoadSoundFromWave(w);
        SetSoundVolume(engine, 0.0f);

        // удар: шум с экспоненциальным затуханием
        int M = SR/3;
        std::vector<short> cb(M);
        for (int i=0;i<M;i++) {
            float t = i/(float)SR;
            float env = std::exp(-t*16.f);
            float n = frand(-1,1);
            float low = std::sin(2*PI*70*t)*std::exp(-t*9.f);
            cb[i] = (short)(clampf(n*env*0.7f + low*0.8f,-1,1)*30000);
        }
        Wave cw{}; cw.frameCount=M; cw.sampleRate=SR; cw.sampleSize=16; cw.channels=1;
        cw.data = cb.data();
        crash = LoadSoundFromWave(cw);

        // визг шин
        int K = SR/2;
        std::vector<short> sb(K);
        float f1=0, f2=0;
        for (int i=0;i<K;i++) {
            float t=i/(float)SR;
            float n=frand(-1,1);
            f1 += (n-f1)*0.35f; f2 += (f1-f2)*0.35f;
            float tone = std::sin(2*PI*1250*t + std::sin(2*PI*7*t)*3.f);
            sb[i]=(short)(clampf(tone*0.35f + (f1-f2)*1.6f,-1,1)*17000);
        }
        Wave sw{}; sw.frameCount=K; sw.sampleRate=SR; sw.sampleSize=16; sw.channels=1;
        sw.data=sb.data();
        screech = LoadSoundFromWave(sw);
        SetSoundVolume(screech, 0.f);
        ok = true;
    }
    void updateEngine(float rpm, float throttle, float redline) {
        if (!ok) return;
        if (!IsSoundPlaying(engine)) PlaySound(engine);
        SetSoundPitch(engine, clampf(0.55f + rpm/redline*2.3f, 0.4f, 3.2f));
        engVol = smoothTo(engVol, 0.10f + throttle*0.22f, 8.f, GetFrameTime());
        SetSoundVolume(engine, engVol);
    }
    void updateScreech(float amt) {
        if (!ok) return;
        if (!IsSoundPlaying(screech)) PlaySound(screech);
        SetSoundVolume(screech, clampf(amt,0,1)*0.28f);
        SetSoundPitch(screech, 0.85f + clampf(amt,0,1)*0.5f);
    }
    void bang(float power) {
        if (!ok) return;
        SetSoundPitch(crash, frand(0.7f,1.3f));
        SetSoundVolume(crash, clampf(power*0.10f, 0.10f, 0.85f));
        PlaySound(crash);
    }
    void shutdown() {
        if (!ok) return;
        UnloadSound(engine); UnloadSound(crash); UnloadSound(screech);
    }
};