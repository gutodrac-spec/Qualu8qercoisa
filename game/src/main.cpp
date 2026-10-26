// Neon Coast - protótipo de mundo aberto (original, inspirado em cidades costeiras tropicais)
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <cmath>
#include <cstdlib>

enum Screen { TITLE, SETTINGS, PLAYING, PAUSED };
enum Upscaler { UPS_OFF, UPS_FSR_LIKE, UPS_DLSS };
static const char* UPS_NAMES[] = { "Desligado (Nativo)", "Escala de Resolucao 75%", "NVIDIA DLSS (requer SDK)" };

struct Building { Vector3 pos, size; Color col; };
struct Palm { Vector3 pos; float h; };
struct Car { Vector3 pos; float yaw, speed; Color col; bool ai; };

static float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 900, "Neon Coast");
    SetExitKey(KEY_NULL);
    SetTargetFPS(144);
    srand(42);

    // Cidade em grade
    std::vector<Building> buildings; std::vector<Palm> palms; std::vector<Car> cars;
    const int GRID = 14; const float BLOCK = 40.f, ROAD = 14.f;
    Color pastel[] = { {255,180,200,255},{140,220,230,255},{250,230,170,255},{200,180,240,255},{240,240,235,255},{255,160,120,255} };
    for (int x = -GRID/2; x < GRID/2; x++) for (int z = -GRID/2; z < GRID/2; z++) {
        float cx = x * (BLOCK + ROAD), cz = z * (BLOCK + ROAD);
        bool beach = x >= GRID/2 - 2;
        if (beach) { if (rand()%2) palms.push_back({{cx+frand(-15,15),0,cz+frand(-15,15)}, frand(7,12)}); continue; }
        int n = 1 + rand() % 3;
        for (int i = 0; i < n; i++) {
            float w = frand(10, BLOCK/n), d = frand(12, BLOCK-4);
            float h = (abs(x) < 3 && abs(z) < 3) ? frand(40, 140) : frand(8, 35);
            buildings.push_back({{cx - BLOCK/2 + (i+0.5f)*BLOCK/n, h/2, cz}, {w, h, d}, pastel[rand()%6]});
        }
        palms.push_back({{cx + BLOCK/2 + 3, 0, cz + BLOCK/2 + 3}, frand(8, 13)});
    }
    for (int i = 0; i < 40; i++) {
        bool alongX = rand()%2; float lane = (rand()%GRID - GRID/2) * (BLOCK + ROAD) + (BLOCK + ROAD)/2;
        cars.push_back({ alongX ? Vector3{frand(-300,300),0.8f,lane} : Vector3{lane,0.8f,frand(-300,300)},
                         alongX ? 90.f*DEG2RAD : 0.f, frand(8,18), pastel[rand()%6], true });
    }
    Car playerCar = {{(BLOCK+ROAD)/2, 0.8f, 0}, 0, 0, {220,30,60,255}, false};

    Camera3D cam = {}; cam.up = {0,1,0}; cam.fovy = 60; cam.projection = CAMERA_PERSPECTIVE;
    Vector3 player = {(BLOCK+ROAD)/2 + 4, 0, 4}; float pyaw = 0, camYaw = 0, camPitch = 0.35f;
    bool inCar = false; Screen screen = TITLE; int menuSel = 0, setSel = 0;
    int upscaler = UPS_OFF; bool shadowsHQ = true; float timeOfDay = 17.5f; int money = 0, wanted = 0;
    RenderTexture2D rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    float rtScale = 1.f; float t = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime(); t += dt;
        int W = GetScreenWidth(), H = GetScreenHeight();
        float wantScale = (upscaler == UPS_FSR_LIKE) ? 0.75f : 1.f;
        if (rt.texture.width != (int)(W*wantScale) || rt.texture.height != (int)(H*wantScale)) {
            UnloadRenderTexture(rt); rt = LoadRenderTexture((int)(W*wantScale), (int)(H*wantScale));
            SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR); rtScale = wantScale;
        }

        // ---------- Lógica ----------
        if (screen == TITLE) {
            if (IsKeyPressed(KEY_DOWN)) menuSel = (menuSel+1)%3;
            if (IsKeyPressed(KEY_UP)) menuSel = (menuSel+2)%3;
            if (IsKeyPressed(KEY_ENTER)) { if (menuSel==0) { screen = PLAYING; DisableCursor(); } else if (menuSel==1) screen = SETTINGS; else break; }
        } else if (screen == SETTINGS) {
            if (IsKeyPressed(KEY_DOWN)) setSel = (setSel+1)%3;
            if (IsKeyPressed(KEY_UP)) setSel = (setSel+2)%3;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
                if (setSel==0) upscaler = (upscaler+1)%3; else if (setSel==1) shadowsHQ = !shadowsHQ; else timeOfDay = fmodf(timeOfDay+3, 24);
            }
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) screen = TITLE;
        } else if (screen == PAUSED) {
            if (IsKeyPressed(KEY_ESCAPE)) { screen = PLAYING; DisableCursor(); }
            if (IsKeyPressed(KEY_Q)) { screen = TITLE; }
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) { screen = PAUSED; EnableCursor(); }
            timeOfDay = fmodf(timeOfDay + dt * 0.02f, 24.f);
            Vector2 md = GetMouseDelta(); camYaw -= md.x * 0.003f; camPitch = Clamp(camPitch + md.y * 0.003f, 0.05f, 1.2f);
            if (IsKeyPressed(KEY_E)) {
                if (inCar) { inCar = false; player = Vector3Add(playerCar.pos, {cosf(playerCar.yaw)*2.5f, -0.8f, -sinf(playerCar.yaw)*2.5f}); }
                else if (Vector3Distance(player, playerCar.pos) < 5) inCar = true;
            }
            if (inCar) {
                float acc = IsKeyDown(KEY_W) ? 18 : IsKeyDown(KEY_S) ? -14 : 0;
                playerCar.speed += acc * dt; playerCar.speed *= (1 - 0.6f*dt);
                if (IsKeyDown(KEY_SPACE)) playerCar.speed *= (1 - 4*dt);
                playerCar.speed = Clamp(playerCar.speed, -12, 55);
                float steer = (IsKeyDown(KEY_A) ? 1 : 0) - (IsKeyDown(KEY_D) ? 1 : 0);
                playerCar.yaw += steer * dt * 1.8f * Clamp(playerCar.speed/10, -1, 1);
                Vector3 np = Vector3Add(playerCar.pos, {sinf(playerCar.yaw)*playerCar.speed*dt, 0, cosf(playerCar.yaw)*playerCar.speed*dt});
                bool hit = false;
                for (auto& b : buildings) if (fabsf(np.x-b.pos.x) < b.size.x/2+1.2f && fabsf(np.z-b.pos.z) < b.size.z/2+1.2f) { hit = true; break; }
                if (hit) playerCar.speed *= -0.3f; else playerCar.pos = np;
                player = playerCar.pos; player.y = 0;
            } else {
                float spd = IsKeyDown(KEY_LEFT_SHIFT) ? 9.f : 4.f;
                Vector3 f = {sinf(camYaw+PI), 0, cosf(camYaw+PI)}, r = {f.z, 0, -f.x};
                Vector3 mv = {0,0,0};
                if (IsKeyDown(KEY_W)) mv = Vector3Add(mv, f); if (IsKeyDown(KEY_S)) mv = Vector3Subtract(mv, f);
                if (IsKeyDown(KEY_A)) mv = Vector3Add(mv, r); if (IsKeyDown(KEY_D)) mv = Vector3Subtract(mv, r);
                if (Vector3Length(mv) > 0) {
                    mv = Vector3Scale(Vector3Normalize(mv), spd*dt); pyaw = atan2f(mv.x, mv.z);
                    Vector3 np = Vector3Add(player, mv); bool hit = false;
                    for (auto& b : buildings) if (fabsf(np.x-b.pos.x) < b.size.x/2+0.4f && fabsf(np.z-b.pos.z) < b.size.z/2+0.4f) { hit = true; break; }
                    if (!hit) player = np;
                }
            }
            for (auto& c : cars) {
                c.pos.x += sinf(c.yaw)*c.speed*dt; c.pos.z += cosf(c.yaw)*c.speed*dt;
                if (fabsf(c.pos.x) > 380 || fabsf(c.pos.z) > 380) c.yaw += PI;
                if (inCar && Vector3Distance(c.pos, playerCar.pos) < 3 && fabsf(playerCar.speed) > 15) { money += 50; wanted = (wanted < 5) ? wanted+1 : 5; c.speed = 0; }
            }
            Vector3 focus = inCar ? playerCar.pos : Vector3Add(player, {0,1.6f,0});
            float dist = inCar ? 11.f : 5.f;
            float yawRef = camYaw;
            cam.target = focus;
            cam.position = Vector3Add(focus, {sinf(yawRef)*cosf(camPitch)*dist, sinf(camPitch)*dist + 1, cosf(yawRef)*cosf(camPitch)*dist});
        }

        // ---------- Iluminação / céu ----------
        float sunA = (timeOfDay - 6) / 12 * PI; float sunH = sinf(sunA);
        float day = Clamp(sunH * 2 + 0.3f, 0, 1);
        Color skyTop = ColorLerp({10,10,35,255}, {60,140,230,255}, day);
        Color skyHor = ColorLerp({60,20,70,255}, ColorLerp({255,130,90,255},{180,220,255,255}, Clamp(sunH*2,0,1)), Clamp(day+0.2f,0,1));
        auto shade = [&](Color c, float k) { float m = 0.25f + 0.75f*day*k; return Color{(unsigned char)(c.r*m),(unsigned char)(c.g*m),(unsigned char)(c.b*m),c.a}; };

        // ---------- Render 3D em render target (escala de resolução) ----------
        BeginTextureMode(rt);
        ClearBackground(skyHor);
        DrawRectangleGradientV(0, 0, rt.texture.width, rt.texture.height/2, skyTop, skyHor);
        if (screen == PLAYING || screen == PAUSED) {
            BeginMode3D(cam);
            DrawPlane({0,-0.01f,0}, {900,900}, shade({70,72,78,255},1));            // asfalto
            DrawPlane({(GRID/2-1.5f)*(BLOCK+ROAD),0,0}, {120,900}, shade({235,215,160,255},1)); // praia
            DrawPlane({(GRID/2+3)*(BLOCK+ROAD),0.05f,0}, {300,900}, shade({30,150,170,200},1)); // oceano
            for (int x = -GRID/2; x < GRID/2-2; x++) for (int z = -GRID/2; z < GRID/2; z++)
                DrawCube({x*(BLOCK+ROAD),0.05f,z*(BLOCK+ROAD)}, BLOCK, 0.15f, BLOCK, shade({150,150,145,255},1)); // calçadas
            for (auto& b : buildings) {
                float side = shadowsHQ ? 0.75f : 1.f;
                DrawCubeV(b.pos, b.size, shade(b.col, 1));
                DrawCubeWiresV(b.pos, b.size, shade(b.col, side*0.6f));
                if (day < 0.5f) for (float y = 3; y < b.size.y-1; y += 4)    // janelas acesas à noite
                    DrawCube({b.pos.x, y, b.pos.z + b.size.z/2 + 0.05f}, b.size.x*0.8f, 1.2f, 0.05f, ((int)(y+b.pos.x))%3 ? Color{255,220,140,255} : Color{40,40,60,255});
                if (shadowsHQ) DrawCube({b.pos.x+2, 0.06f, b.pos.z+2}, b.size.x, 0.02f, b.size.z, Fade(BLACK, 0.25f*day));
            }
            for (auto& p : palms) {
                DrawCylinder(p.pos, 0.25f, 0.35f, p.h, 8, shade({120,90,60,255},1));
                for (int i = 0; i < 6; i++) { float a = i*PI/3 + sinf(t+p.pos.x)*0.05f;
                    DrawCylinderEx({p.pos.x,p.h,p.pos.z}, {p.pos.x+cosf(a)*3.5f, p.h-1.2f, p.pos.z+sinf(a)*3.5f}, 0.5f, 0.05f, 4, shade({40,140,60,255},1)); }
            }
            auto drawCar = [&](const Car& c) {
                rlPushMatrix(); 
                DrawCubeV(c.pos, {2.0f, 0.8f, 4.2f}, shade(c.col,1));
                DrawCubeV({c.pos.x, c.pos.y+0.65f, c.pos.z}, {1.7f, 0.6f, 2.0f}, shade({30,40,60,255},1));
                if (day < 0.5f) { DrawSphere({c.pos.x+sinf(c.yaw)*2.1f, c.pos.y, c.pos.z+cosf(c.yaw)*2.1f}, 0.3f, {255,250,210,255}); }
                rlPopMatrix();
            };
            for (auto& c : cars) drawCar(c);
            drawCar(playerCar);
            if (!inCar) {
                DrawCapsule({player.x,0.4f,player.z}, {player.x,1.5f,player.z}, 0.35f, 8, 8, shade({240,200,170,255},1));
                DrawCube({player.x,1.0f,player.z}, 0.75f, 0.7f, 0.5f, shade({40,90,180,255},1));
            }
            EndMode3D();
        }
        EndTextureMode();

        // ---------- Composição na tela ----------
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(rt.texture, {0,0,(float)rt.texture.width,-(float)rt.texture.height}, {0,0,(float)W,(float)H}, {0,0}, 0, WHITE);
        // Vinheta / color grading quente
        DrawRectangleGradientV(0, H*0.6f, W, H*0.4f, BLANK, Fade({60,0,40,255}, 0.35f));
        DrawRectangle(0,0,W,H, Fade({255,120,60,255}, 0.06f*(1-day)+0.04f));

        if (screen == TITLE || screen == SETTINGS) {
            DrawRectangleGradientH(0,0,W,H, Fade({255,60,140,255},0.55f), Fade({40,200,230,255},0.45f));
            for (int i = 0; i < 8; i++) DrawCircle(W*0.75f, H*0.45f, 220 - i*20, Fade({255,200,80,255}, 0.08f));
            DrawText("NEON COAST", 100, 120, 110, WHITE);
            DrawText("um prototipo de mundo aberto", 106, 240, 30, Fade(WHITE,0.85f));
            if (screen == TITLE) {
                const char* items[] = {"JOGAR", "CONFIGURACOES GRAFICAS", "SAIR"};
                for (int i = 0; i < 3; i++) DrawText(items[i], 110, 380+i*60, 40, i==menuSel ? YELLOW : WHITE);
            } else {
                const char* lbl[] = {"Upscaling", "Sombras", "Hora do dia"};
                const char* val[] = { UPS_NAMES[upscaler], shadowsHQ ? "Alta" : "Baixa", TextFormat("%02d:00", (int)timeOfDay) };
                for (int i = 0; i < 3; i++) DrawText(TextFormat("%-14s < %s >", lbl[i], val[i]), 110, 380+i*60, 36, i==setSel ? YELLOW : WHITE);
                if (upscaler == UPS_DLSS) DrawText("DLSS precisa do NVIDIA Streamline SDK + RTX. Sem SDK, roda nativo.", 110, 580, 24, ORANGE);
                DrawText("ENTER/-> alterar   ESC voltar", 110, 640, 24, Fade(WHITE,0.7f));
            }
            DrawText("ENTER selecionar  |  setas navegar", 110, H-60, 22, Fade(WHITE,0.7f));
        } else {
            DrawText(TextFormat("$%d", money), W-220, 30, 44, {120,255,140,255});
            for (int i = 0; i < 5; i++) DrawText("*", W-220+i*36, 80, 50, i < wanted ? GOLD : Fade(WHITE,0.3f));
            DrawText(TextFormat("%02d:%02d", (int)timeOfDay, (int)(fmodf(timeOfDay,1)*60)), W-220, 140, 30, WHITE);
            DrawCircle(110, H-110, 90, Fade(BLACK,0.5f)); // minimapa
            for (auto& b : buildings) { float mx = 110 + (b.pos.x-player.x)*0.25f, mz = H-110 + (b.pos.z-player.z)*0.25f;
                if ((mx-110)*(mx-110)+(mz-H+110)*(mz-H+110) < 85*85) DrawRectangle(mx-2, mz-2, 4, 4, Fade(WHITE,0.5f)); }
            DrawCircle(110, H-110, 5, YELLOW);
            DrawText(inCar ? TextFormat("%d km/h", (int)fabsf(playerCar.speed*3.6f)) : "E: entrar no carro (perto)", 220, H-50, 26, WHITE);
            DrawFPS(10,10);
            if (screen == PAUSED) { DrawRectangle(0,0,W,H,Fade(BLACK,0.6f)); DrawText("PAUSADO", W/2-150, H/2-60, 70, WHITE); DrawText("ESC continuar  |  Q menu", W/2-160, H/2+30, 28, WHITE); }
        }
        EndDrawing();
    }
    UnloadRenderTexture(rt);
    CloseWindow();
    return 0;
}
