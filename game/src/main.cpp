// Neon Coast - jogo de mundo aberto em uma cidade costeira tropical (C++ / raylib / GLSL 330)
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <utility>

// =====================================================================================
// Shaders
// =====================================================================================
static const char* LIT_VS = R"(#version 330
in vec3 vertexPosition; in vec3 vertexNormal;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;
out vec3 fragPos; out vec3 fragNormal;
void main(){
    fragPos = (matModel*vec4(vertexPosition,1.0)).xyz;
    fragNormal = normalize((matNormal*vec4(vertexNormal,0.0)).xyz);
    gl_Position = mvp*vec4(vertexPosition,1.0);
})";

static const char* LIT_FS = R"(#version 330
in vec3 fragPos; in vec3 fragNormal;
uniform vec4 colDiffuse;
uniform vec3 lightDir, lightColor, ambSky, ambGround, viewPos, fogColor;
uniform float fogDensity, night, time, emissive, roughness;
uniform int surface;            // 0 generico, 1 terreno (asfalto/areia/agua), 2 fachada com janelas
uniform mat4 lightVP; uniform sampler2D shadowMap; uniform int shadowsOn;
uniform float blockP, roadW, beachX, waterX;
uniform vec3 headPos, headDir; uniform float headOn;
out vec4 finalColor;

float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898,78.233)))*43758.5453); }
float noise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y); }
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }

float shadow(vec3 n){
    if (shadowsOn==0) return 1.0;
    vec4 lp = lightVP*vec4(fragPos,1.0); vec3 p = lp.xyz/lp.w*0.5+0.5;
    if (p.x<0.0||p.x>1.0||p.y<0.0||p.y>1.0||p.z>1.0) return 1.0;
    float bias = max(0.0025*(1.0-dot(n,lightDir)), 0.0006);
    vec2 texel = 1.0/vec2(textureSize(shadowMap,0)); float s=0.0;
    for(int x=-2;x<=2;x++) for(int y=-2;y<=2;y++)
        s += (p.z-bias > texture(shadowMap, p.xy+vec2(x,y)*texel).r) ? 0.0 : 1.0;
    return s/25.0;
}

void main(){
    vec3 n = normalize(fragNormal);
    vec3 albedo = pow(colDiffuse.rgb, vec3(2.2));
    vec3 V = normalize(viewPos-fragPos);
    float rough = roughness, metal = 0.0; vec3 emis = vec3(0.0);
    bool water = false;

    if (surface==1){
        float x = fragPos.x, z = fragPos.z;
        if (x > waterX){
            water = true;
            float t = time;
            vec2 w = fragPos.xz;
            float dx = cos(w.x*0.35+t*1.3)*0.08 + cos(w.x*1.3+w.y*0.7+t*2.1)*0.04 + (noise(w*0.8+t)-0.5)*0.08;
            float dz = cos(w.y*0.4+t*1.1)*0.08 + cos(w.y*1.7-w.x*0.5+t*1.7)*0.04 + (noise(w*0.8-t)-0.5)*0.08;
            n = normalize(vec3(-dx,1.0,-dz));
            float shore = clamp((x-waterX)/40.0,0.0,1.0);
            albedo = mix(vec3(0.05,0.35,0.35), vec3(0.01,0.08,0.15), shore);
            rough = 0.05;
            float foam = smoothstep(0.75,1.0, noise(vec2(z*0.2, x*0.5 - t*0.6))) * (1.0-smoothstep(0.0,6.0,x-waterX));
            albedo = mix(albedo, vec3(0.9), foam);
        } else if (x > beachX){
            float wet = smoothstep(waterX-8.0, waterX, x);
            albedo = mix(vec3(0.85,0.72,0.5), vec3(0.45,0.36,0.25), wet) * (0.9+0.2*noise(fragPos.xz*3.0));
            rough = mix(0.95, 0.3, wet);
        } else {
            float g = noise(fragPos.xz*2.0)*0.5+noise(fragPos.xz*9.0)*0.5;
            albedo = vec3(0.06,0.065,0.07)*(0.8+0.4*g);
            rough = 0.7 - 0.25*noise(fragPos.xz*0.15);
            float ax = abs(mod(x, blockP)-blockP*0.5), az = abs(mod(z, blockP)-blockP*0.5);
            bool roadX = az < roadW*0.5, roadZ = ax < roadW*0.5;
            if (roadZ && !roadX && ax<0.12 && fract(z/6.0)<0.55) albedo = vec3(0.6,0.45,0.05);
            if (roadX && !roadZ && az<0.12 && fract(x/6.0)<0.55) albedo = vec3(0.6,0.45,0.05);
            if (roadZ && !roadX && abs(ax-roadW*0.5+0.6)<0.08) albedo = vec3(0.55);
            if (roadX && !roadZ && abs(az-roadW*0.5+0.6)<0.08) albedo = vec3(0.55);
            if (roadX && roadZ && (fract((x+z)*0.5)<0.5) && (ax>roadW*0.35 || az>roadW*0.35)) albedo = vec3(0.5);
        }
    } else if (surface==2 && abs(n.y)<0.5){
        float u = abs(n.x)>abs(n.z) ? fragPos.z : fragPos.x;
        vec2 cell = floor(vec2(u/3.0, fragPos.y/3.6));
        vec2 f = fract(vec2(u/3.0, fragPos.y/3.6));
        float win = step(0.18,f.x)*step(f.x,0.82)*step(0.22,f.y)*step(f.y,0.85)*step(3.0,fragPos.y);
        float r = hash(cell + floor(fragPos.xz/7.0)*13.1);
        albedo = mix(albedo, vec3(0.03,0.05,0.08), win);
        rough = mix(rough, 0.08, win); metal = win*0.6;
        emis += win * step(0.55, r) * night * mix(vec3(1.0,0.7,0.35), vec3(0.7,0.85,1.0), step(0.85,r)) * 1.6;
        albedo *= 0.92 + 0.08*noise(fragPos.xy*0.5 + fragPos.zy*0.5);
    }

    vec3 L = normalize(lightDir), H = normalize(L+V);
    float NdL = max(dot(n,L),0.0);
    float sh = shadow(normalize(fragNormal));
    float spec = pow(max(dot(n,H),0.0), mix(512.0,8.0,rough)) * (1.0-rough) * 2.0;
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    float fres = pow(1.0-max(dot(n,V),0.0), 5.0);
    vec3 F = F0 + (1.0-F0)*fres;

    vec3 amb = mix(ambGround, ambSky, n.y*0.5+0.5) * albedo;
    vec3 col = amb + albedo*lightColor*NdL*sh*(1.0-metal) + lightColor*spec*sh*F;
    vec3 R = reflect(-V,n);
    vec3 env = mix(fogColor, ambSky*2.2, clamp(R.y,0.0,1.0));
    col += env * F * (1.0-rough) * (water?1.2:0.6);

    // postes de luz procedurais nos cruzamentos
    vec2 c = (floor(fragPos.xz/blockP)+0.5)*blockP;
    vec3 lamp = vec3(c.x+4.0, 7.3, c.y+4.0);
    vec3 dl = lamp-fragPos; float d2 = dot(dl,dl);
    col += albedo * vec3(1.0,0.6,0.25) * 60.0 / (d2+4.0) * max(dot(n,normalize(dl)),0.0) * night;
    // farol do carro do jogador
    if (headOn>0.5){
        vec3 hd = fragPos-headPos; float hl = length(hd);
        float cone = smoothstep(0.82,0.95, dot(hd/hl, headDir));
        col += albedo * vec3(1.0,0.95,0.85) * cone * 120.0/(hl*hl+6.0) * max(dot(n,-hd/hl),0.0);
    }

    col += emis + albedo*emissive;
    float dist = length(viewPos-fragPos);
    float fog = 1.0 - exp(-pow(dist*fogDensity, 1.5));
    col = mix(col, fogColor, clamp(fog,0.0,1.0));
    col = aces(col*1.1);
    finalColor = vec4(pow(col, vec3(1.0/2.2)), 1.0);
})";

static const char* SKY_FS = R"(#version 330
in vec3 fragPos; in vec3 fragNormal;
uniform vec3 viewPos, sunDir, sunColor, horizon, zenith; uniform float time, night;
out vec4 finalColor;
float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898,78.233)))*43758.5453); }
float noise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y); }
float fbm(vec2 p){ float v=0.0,a=0.5; for(int i=0;i<5;i++){ v+=a*noise(p); p*=2.03; a*=0.5; } return v; }
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }
void main(){
    vec3 d = normalize(fragPos-viewPos);
    float h = clamp(d.y,0.0,1.0);
    vec3 col = mix(horizon, zenith, pow(h,0.45));
    float sd = max(dot(d,normalize(sunDir)),0.0);
    col += sunColor*(pow(sd,6.0)*0.35 + pow(sd,64.0)*0.8) + sunColor*smoothstep(0.9993,0.9997,sd)*20.0;
    if (d.y>0.0){
        vec2 uv = d.xz/(d.y+0.15)*1.5 + vec2(time*0.01,0.0);
        float cl = smoothstep(0.45,0.85, fbm(uv));
        vec3 cc = mix(horizon*1.3, vec3(1.0), 0.5) * (0.5+0.6*pow(sd,3.0)) + sunColor*pow(sd,10.0)*0.6;
        col = mix(col, cc*(1.0-night*0.85), cl*smoothstep(0.0,0.15,d.y)*0.85);
        float st = step(0.9985, hash(floor(d.xz/(d.y+0.3)*400.0)));
        col += vec3(st)*night*(1.0-cl)*1.5;
    } else col = mix(horizon, horizon*0.4, clamp(-d.y*4.0,0.0,1.0));
    col = aces(col);
    finalColor = vec4(pow(col, vec3(1.0/2.2)),1.0);
})";

static const char* DEPTH_FS = R"(#version 330
out vec4 c; void main(){ c = vec4(1.0); })";

// Pós-processamento: nitidez (upscale), bloom, aberração cromática, vinheta, grão
static const char* POST_FS = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; uniform vec2 srcRes; uniform float sharpen, time, bloomOn;
out vec4 finalColor;
float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898,78.233)))*43758.5453); }
void main(){
    vec2 uv = fragTexCoord; vec2 px = 1.0/srcRes;
    vec2 cc = uv-0.5;
    vec3 c;
    c.r = texture(texture0, uv - cc*0.0025).r;
    c.g = texture(texture0, uv).g;
    c.b = texture(texture0, uv + cc*0.0025).b;
    if (sharpen>0.0){
        vec3 nb = texture(texture0,uv+vec2(px.x,0)).rgb + texture(texture0,uv-vec2(px.x,0)).rgb
                + texture(texture0,uv+vec2(0,px.y)).rgb + texture(texture0,uv-vec2(0,px.y)).rgb;
        c = clamp(c + (c*4.0-nb)*sharpen*0.25, 0.0, 1.0);
    }
    if (bloomOn>0.5){
        vec3 b = vec3(0.0);
        for(int i=0;i<16;i++){
            float a = float(i)*0.3927; float r = (i<8)?6.0:14.0;
            vec3 s = texture(texture0, uv+vec2(cos(a),sin(a))*px*r).rgb;
            b += max(s-0.7,0.0);
        }
        c += b/16.0*1.6;
    }
    c *= 1.0 - dot(cc,cc)*0.9;
    c += (hash(uv*srcRes+time)-0.5)*0.025;
    finalColor = vec4(c,1.0);
})";

// =====================================================================================
// Mundo
// =====================================================================================
struct Building { Vector3 pos, size; Color col; };
struct Palm { Vector3 pos; float h, lean; };
struct Car { Vector3 pos; float yaw, speed; Color col; int type; bool police; float stun; };
struct Ped { Vector3 pos; float yaw, speed, phase; Color shirt; bool dead; };

static float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }

static const int GRID = 16;
static const float BLOCK = 40.f, ROAD = 14.f, P = BLOCK + ROAD;

static Model mCube, mCyl, mSphere, mPlane;
static Shader lit, sky, depth, post;
static bool shadowPass = false;
static int locEmissive, locSurface, locRough;

static void setUniF(int loc, float v) { SetShaderValue(lit, loc, &v, SHADER_UNIFORM_FLOAT); }
static void setUniI(int loc, int v) { SetShaderValue(lit, loc, &v, SHADER_UNIFORM_INT); }

static void drawM(Model& m, Matrix world, Color c, float emissive = 0, float rough = 0.6f, int surface = 0) {
    if (!shadowPass) { setUniF(locEmissive, emissive); setUniF(locRough, rough); setUniI(locSurface, surface); }
    m.transform = world;
    DrawModel(m, {0, 0, 0}, 1.f, c);
}
static Matrix TRS(Vector3 t, float yaw, Vector3 s) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), MatrixRotateY(yaw)), MatrixTranslate(t.x, t.y, t.z));
}
static Matrix local(Vector3 base, float yaw, Vector3 off, Vector3 s, Matrix pre = MatrixIdentity()) {
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), pre), MatrixTranslate(off.x, off.y, off.z));
    return MatrixMultiply(m, MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(base.x, base.y, base.z)));
}

static void drawCar(const Car& c, float t, float night) {
    Vector3 b = {c.pos.x, 0, c.pos.z};
    float L = c.type == 1 ? 4.8f : 4.3f, W = 1.9f;
    Color body = c.police ? Color{20, 20, 25, 255} : c.col;
    drawM(mCube, local(b, c.yaw, {0, 0.75f, 0}, {W, 0.65f, L}), body, 0, 0.15f);
    drawM(mCube, local(b, c.yaw, {0, 1.3f, -0.2f}, {W * 0.88f, 0.55f, L * 0.48f}), {15, 20, 28, 255}, 0, 0.05f);
    if (c.police) drawM(mCube, local(b, c.yaw, {0, 0.76f, 0}, {W + 0.02f, 0.3f, L * 0.5f}), RAYWHITE, 0, 0.2f);
    float spin = -c.pos.x * 0.0f;
    for (int i = 0; i < 4; i++) {
        float sx = (i & 1) ? W / 2 : -W / 2, sz = (i & 2) ? L * 0.33f : -L * 0.33f;
        drawM(mCyl, local(b, c.yaw, {sx, 0.38f, sz}, {1, 1, 1}, MatrixMultiply(MatrixTranslate(0, -0.13f, 0), MatrixRotateZ(PI / 2 + spin))), {20, 20, 20, 255}, 0, 0.9f);
    }
    float hl = 0.3f + night * 3.f;
    drawM(mCube, local(b, c.yaw, {-W * 0.32f, 0.85f, L / 2}, {0.4f, 0.15f, 0.05f}), {255, 250, 230, 255}, hl, 0.1f);
    drawM(mCube, local(b, c.yaw, {W * 0.32f, 0.85f, L / 2}, {0.4f, 0.15f, 0.05f}), {255, 250, 230, 255}, hl, 0.1f);
    drawM(mCube, local(b, c.yaw, {-W * 0.35f, 0.85f, -L / 2}, {0.4f, 0.12f, 0.05f}), {255, 20, 20, 255}, 0.5f + night * 2, 0.1f);
    drawM(mCube, local(b, c.yaw, {W * 0.35f, 0.85f, -L / 2}, {0.4f, 0.12f, 0.05f}), {255, 20, 20, 255}, 0.5f + night * 2, 0.1f);
    if (c.police) {
        bool ph = fmodf(t * 4, 1) < 0.5f;
        drawM(mCube, local(b, c.yaw, {-0.35f, 1.65f, -0.2f}, {0.6f, 0.15f, 0.25f}), {255, 30, 30, 255}, ph ? 8.f : 0.3f);
        drawM(mCube, local(b, c.yaw, {0.35f, 1.65f, -0.2f}, {0.6f, 0.15f, 0.25f}), {30, 60, 255, 255}, ph ? 0.3f : 8.f);
    }
}

static void drawHuman(Vector3 p, float yaw, float phase, float moving, Color shirt, Color pants, Color skin) {
    float sw = sinf(phase) * 0.6f * moving, bob = fabsf(sinf(phase)) * 0.05f * moving;
    Vector3 b = {p.x, bob, p.z};
    drawM(mCube, local(b, yaw, {0, 1.25f, 0}, {0.5f, 0.6f, 0.28f}), shirt, 0, 0.8f);
    drawM(mSphere, local(b, yaw, {0, 1.75f, 0}, {0.2f, 0.24f, 0.22f}), skin, 0, 0.6f);
    drawM(mSphere, local(b, yaw, {0, 1.86f, -0.02f}, {0.21f, 0.12f, 0.22f}), {30, 20, 15, 255}, 0, 0.9f);
    for (int s = -1; s <= 1; s += 2) {
        Matrix leg = MatrixMultiply(MatrixTranslate(0, -0.42f, 0), MatrixRotateX(sw * s));
        drawM(mCube, local(b, yaw, {0.13f * s, 0.88f, 0}, {0.19f, 0.84f, 0.22f}, leg), pants, 0, 0.85f);
        Matrix arm = MatrixMultiply(MatrixTranslate(0, -0.3f, 0), MatrixRotateX(-sw * s * 0.8f));
        drawM(mCube, local(b, yaw, {0.33f * s, 1.52f, 0}, {0.13f, 0.6f, 0.14f}, arm), skin, 0, 0.6f);
    }
}

static RenderTexture2D LoadShadowmap(int size) {
    RenderTexture2D t = {0};
    t.id = rlLoadFramebuffer();
    t.texture.width = t.texture.height = size;
    rlEnableFramebuffer(t.id);
    t.depth.id = rlLoadTextureDepth(size, size, false);
    t.depth.width = t.depth.height = size; t.depth.format = 19; t.depth.mipmaps = 1;
    rlFramebufferAttach(t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferComplete(t.id);
    rlDisableFramebuffer();
    return t;
}

enum Screen { TITLE, SETTINGS, PLAYING, PAUSED, BUSTED };
enum Upscaler { UPS_NATIVE, UPS_QUALITY, UPS_PERF, UPS_DLSS };
static const char* UPS_NAMES[] = {"Nativo (100%)", "Qualidade (67% + nitidez)", "Desempenho (50% + nitidez)", "NVIDIA DLSS"};

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(1600, 900, "Neon Coast");
    int mon = GetCurrentMonitor();
    SetWindowSize(GetMonitorWidth(mon), GetMonitorHeight(mon));
    ToggleBorderlessWindowed();
    SetExitKey(KEY_NULL);
    srand(7);

    lit = LoadShaderFromMemory(LIT_VS, LIT_FS);
    sky = LoadShaderFromMemory(LIT_VS, SKY_FS);
    depth = LoadShaderFromMemory(LIT_VS, DEPTH_FS);
    post = LoadShaderFromMemory(nullptr, POST_FS);
    lit.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(lit, "matModel");
    lit.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(lit, "matNormal");
    sky.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(sky, "matModel");
    depth.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(depth, "matModel");
    locEmissive = GetShaderLocation(lit, "emissive");
    locSurface = GetShaderLocation(lit, "surface");
    locRough = GetShaderLocation(lit, "roughness");
    auto L = [&](const char* n) { return GetShaderLocation(lit, n); };
    int lLightDir = L("lightDir"), lLightCol = L("lightColor"), lAmbS = L("ambSky"), lAmbG = L("ambGround"), lView = L("viewPos"),
        lFog = L("fogColor"), lFogD = L("fogDensity"), lNight = L("night"), lTime = L("time"), lLVP = L("lightVP"), lShadow = L("shadowMap"),
        lShOn = L("shadowsOn"), lHeadP = L("headPos"), lHeadD = L("headDir"), lHeadOn = L("headOn");
    float fv;
    fv = P; SetShaderValue(lit, L("blockP"), &fv, SHADER_UNIFORM_FLOAT);
    fv = ROAD; SetShaderValue(lit, L("roadW"), &fv, SHADER_UNIFORM_FLOAT);
    float beachX = (GRID / 2 - 3) * P + P / 2, waterX = beachX + 60;
    SetShaderValue(lit, L("beachX"), &beachX, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L("waterX"), &waterX, SHADER_UNIFORM_FLOAT);
    int sView = GetShaderLocation(sky, "viewPos"), sSun = GetShaderLocation(sky, "sunDir"), sSunC = GetShaderLocation(sky, "sunColor"),
        sHor = GetShaderLocation(sky, "horizon"), sZen = GetShaderLocation(sky, "zenith"), sTime = GetShaderLocation(sky, "time"), sNight = GetShaderLocation(sky, "night");
    int pRes = GetShaderLocation(post, "srcRes"), pSharp = GetShaderLocation(post, "sharpen"), pTime = GetShaderLocation(post, "time"), pBloom = GetShaderLocation(post, "bloomOn");

    mCube = LoadModelFromMesh(GenMeshCube(1, 1, 1));
    mCyl = LoadModelFromMesh(GenMeshCylinder(0.38f, 0.26f, 16));
    mSphere = LoadModelFromMesh(GenMeshSphere(1, 16, 16));
    mPlane = LoadModelFromMesh(GenMeshPlane(1, 1, 64, 64));
    Model mSky = LoadModelFromMesh(GenMeshSphere(1, 32, 32));
    Model* all[] = {&mCube, &mCyl, &mSphere, &mPlane};
    mSky.materials[0].shader = sky;
    RenderTexture2D shadowMap = LoadShadowmap(4096);

    // ---------- Geração da cidade ----------
    std::vector<Building> buildings; std::vector<Palm> palms; std::vector<Car> cars; std::vector<Ped> peds;
    Color pastel[] = {{245, 190, 200, 255}, {160, 215, 220, 255}, {245, 228, 185, 255}, {205, 190, 235, 255}, {238, 236, 228, 255}, {250, 175, 140, 255}, {190, 200, 205, 255}};
    for (int x = -GRID / 2; x < GRID / 2; x++) for (int z = -GRID / 2; z < GRID / 2; z++) {
        float cx = x * P, cz = z * P;
        if (cx > beachX - P / 2) {
            if (cx < waterX) for (int i = 0; i < 3; i++) palms.push_back({{cx + frand(-18, 18), 0, cz + frand(-20, 20)}, frand(8, 13), frand(-0.15f, 0.15f)});
            continue;
        }
        float dc = sqrtf((float)(x * x + z * z));
        bool downtown = dc < 3.5f;
        int nx = downtown ? 1 + rand() % 2 : 2, nz = downtown ? 1 : 1 + rand() % 2;
        for (int i = 0; i < nx; i++) for (int j = 0; j < nz; j++) {
            float w = BLOCK / nx - frand(2, 6), d = BLOCK / nz - frand(2, 6);
            float h = downtown ? frand(50, 170) / (1 + dc * 0.25f) + 20 : frand(7, 28);
            Vector3 pos = {cx - BLOCK / 2 + (i + 0.5f) * BLOCK / nx, h / 2, cz - BLOCK / 2 + (j + 0.5f) * BLOCK / nz};
            buildings.push_back({pos, {w, h, d}, downtown && rand() % 2 ? Color{180, 195, 205, 255} : pastel[rand() % 7]});
        }
        if (!downtown) palms.push_back({{cx + BLOCK / 2 - 1.5f, 0.15f, cz + frand(-BLOCK / 2, BLOCK / 2)}, frand(8, 12), frand(-0.1f, 0.1f)});
    }
    auto spawnCar = [&](bool police) {
        bool alongX = rand() % 2; int k = rand() % GRID - GRID / 2;
        float lane = k * P + P / 2 + (rand() % 2 ? 3.5f : -3.5f);
        float dir = (lane - (k * P + P / 2)) > 0 ? 1.f : -1.f;
        Car c = {alongX ? Vector3{frand(-400, beachX - 40), 0, lane} : Vector3{lane, 0, frand(-400, 400)},
                 alongX ? (dir > 0 ? PI / 2 : -PI / 2) : (dir > 0 ? 0 : PI), frand(9, 16), pastel[rand() % 7], rand() % 3 == 0, police, 0};
        return c;
    };
    for (int i = 0; i < 70; i++) cars.push_back(spawnCar(false));
    for (int i = 0; i < 90; i++) {
        int k = rand() % GRID - GRID / 2; float s = (rand() % 2 ? 1 : -1) * (BLOCK / 2 + 1.5f);
        bool alongX = rand() % 2;
        Vector3 p = alongX ? Vector3{frand(-400, beachX - 40), 0.15f, k * P + s} : Vector3{k * P + s, 0.15f, frand(-400, 400)};
        peds.push_back({p, alongX ? PI / 2 * (rand() % 2 ? 1 : -1) : (rand() % 2 ? 0 : PI), frand(1.1f, 1.7f), frand(0, 6), pastel[rand() % 7], false});
    }
    Car playerCar = {{P / 2 + 3.5f, 0, 10}, 0, 0, {200, 20, 40, 255}, 1, false, 0};

    Camera3D cam = {}; cam.up = {0, 1, 0}; cam.fovy = 55; cam.projection = CAMERA_PERSPECTIVE;
    Vector3 player = {P / 2 + 7.5f, 0.15f, 14}; float pyaw = PI, camYaw = 0, camPitch = 0.3f, walkPhase = 0, moving = 0;
    bool inCar = false; Screen screen = TITLE; int menuSel = 0, setSel = 0;
    int upscaler = UPS_QUALITY; bool shadowsOn = true, bloomOn = true; float timeOfDay = 18.2f;
    int money = 500, wanted = 0; float wantedTimer = 0, health = 100, t = 0, titleT = 0;
    float shake = 0; int skipMouse = 5;
    RenderTexture2D rt = {0};
    if (getenv("NC_PLAY")) screen = PLAYING;
    if (getenv("NC_TOD")) timeOfDay = (float)atof(getenv("NC_TOD"));

    while (!WindowShouldClose()) {
        float dt = fminf(GetFrameTime(), 0.05f); t += dt;
        int W = GetScreenWidth(), H = GetScreenHeight();
        float scale = upscaler == UPS_QUALITY ? 0.67f : upscaler == UPS_PERF ? 0.5f : 1.f;
        int rw = (int)(W * scale), rh = (int)(H * scale);
        if (rt.texture.width != rw || rt.texture.height != rh) {
            if (rt.id) UnloadRenderTexture(rt);
            rt = LoadRenderTexture(rw, rh); SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
        }

        // =============================== Lógica ===============================
        if (screen == TITLE) {
            titleT += dt;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) menuSel = (menuSel + 1) % 3;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) menuSel = (menuSel + 2) % 3;
            if (IsKeyPressed(KEY_ENTER)) { if (menuSel == 0) { screen = PLAYING; DisableCursor(); skipMouse = 5; } else if (menuSel == 1) screen = SETTINGS; else break; }
            float a = titleT * 0.05f;
            cam.target = {0, 40, 0};
            cam.position = {cosf(a) * 380, 110 + sinf(titleT * 0.1f) * 25, sinf(a) * 380};
        } else if (screen == SETTINGS) {
            if (IsKeyPressed(KEY_DOWN)) setSel = (setSel + 1) % 4;
            if (IsKeyPressed(KEY_UP)) setSel = (setSel + 3) % 4;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
                if (setSel == 0) upscaler = (upscaler + 1) % 4;
                else if (setSel == 1) shadowsOn = !shadowsOn;
                else if (setSel == 2) bloomOn = !bloomOn;
                else timeOfDay = fmodf(timeOfDay + 2, 24);
            }
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) screen = TITLE;
        } else if (screen == PAUSED || screen == BUSTED) {
            if (IsKeyPressed(KEY_ESCAPE) || (screen == BUSTED && IsKeyPressed(KEY_ENTER))) {
                if (screen == BUSTED) { wanted = 0; health = 100; money = money / 2; inCar = false; player = {P / 2 + 7.5f, 0.15f, 14};
                    for (auto it = cars.begin(); it != cars.end();) it = it->police ? cars.erase(it) : it + 1; }
                screen = PLAYING; DisableCursor(); skipMouse = 5;
            }
            if (IsKeyPressed(KEY_Q)) { screen = TITLE; EnableCursor(); }
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) { screen = PAUSED; EnableCursor(); }
            timeOfDay = fmodf(timeOfDay + dt * 0.016f, 24.f);
            Vector2 md = GetMouseDelta();
            if (skipMouse > 0 || fabsf(md.x) > 300 || fabsf(md.y) > 300) { md = {0, 0}; skipMouse--; } camYaw -= md.x * 0.0025f; camPitch = Clamp(camPitch + md.y * 0.0025f, -0.1f, 0.9f);
            if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_F)) {
                if (inCar) { inCar = false; player = Vector3Add(playerCar.pos, {cosf(playerCar.yaw) * 2.f, 0.15f, -sinf(playerCar.yaw) * 2.f}); }
                else {
                    Car* best = Vector3Distance(player, playerCar.pos) < 4.5f ? &playerCar : nullptr;
                    for (auto& c : cars) if (!best && !c.police && Vector3Distance(player, c.pos) < 4.f) {   // roubar carro
                        std::swap(c, playerCar); playerCar.speed = 0; c.speed = 0; c.stun = 999; best = &playerCar;
                        if (wanted < 1) wanted = 1; wantedTimer = 0;
                    }
                    if (best) inCar = true;
                }
            }
            auto blocked = [&](Vector3 p, float r) {
                for (auto& b : buildings) if (fabsf(p.x - b.pos.x) < b.size.x / 2 + r && fabsf(p.z - b.pos.z) < b.size.z / 2 + r) return true;
                return p.x > waterX + 2 || fabsf(p.z) > 450 || p.x < -450;
            };
            if (inCar) {
                Car& c = playerCar;
                float throttle = IsKeyDown(KEY_W) ? 1.f : 0.f, brake = IsKeyDown(KEY_S) ? 1.f : 0.f;
                float acc = throttle * (c.speed < 0 ? 30 : 16) - brake * (c.speed > 0.5f ? 28 : 9);
                c.speed += acc * dt;
                c.speed -= c.speed * (0.25f + 0.002f * fabsf(c.speed)) * dt;
                if (IsKeyDown(KEY_SPACE)) c.speed *= (1 - 3.5f * dt);
                c.speed = Clamp(c.speed, -12, 62);
                float steer = (IsKeyDown(KEY_A) ? 1.f : 0.f) - (IsKeyDown(KEY_D) ? 1.f : 0.f);
                float grip = 1.f / (1 + fabsf(c.speed) * 0.025f);
                c.yaw += steer * dt * 2.2f * grip * Clamp(c.speed / 6, -1, 1);
                Vector3 np = Vector3Add(c.pos, {sinf(c.yaw) * c.speed * dt, 0, cosf(c.yaw) * c.speed * dt});
                if (blocked(np, 1.3f)) { if (fabsf(c.speed) > 15) shake = 0.6f; c.speed *= -0.25f; } else c.pos = np;
                for (auto& o : cars) if (Vector2Distance({o.pos.x, o.pos.z}, {c.pos.x, c.pos.z}) < 3.0f) {
                    if (fabsf(c.speed) > 12) { shake = 0.5f; if (!o.police) { money += 25; if (wanted < 2) wanted = 2; } wantedTimer = 0; }
                    o.stun = 3; o.speed = 0; c.speed *= 0.6f;
                    Vector3 push = Vector3Normalize(Vector3Subtract(o.pos, c.pos)); o.pos = Vector3Add(o.pos, Vector3Scale(push, 0.5f));
                }
                for (auto& p : peds) if (!p.dead && Vector3Distance(p.pos, c.pos) < 2.2f && fabsf(c.speed) > 4) { p.dead = true; wanted = wanted < 3 ? 3 : wanted; wantedTimer = 0; }
                player = c.pos; moving = 0;
            } else {
                bool run = IsKeyDown(KEY_LEFT_SHIFT); float spd = run ? 7.5f : 3.2f;
                Vector3 f = {-sinf(camYaw), 0, -cosf(camYaw)}, r = {f.z, 0, -f.x};
                Vector3 mv = {0, 0, 0};
                if (IsKeyDown(KEY_W)) mv = Vector3Add(mv, f); if (IsKeyDown(KEY_S)) mv = Vector3Subtract(mv, f);
                if (IsKeyDown(KEY_A)) mv = Vector3Add(mv, r); if (IsKeyDown(KEY_D)) mv = Vector3Subtract(mv, r);
                if (Vector3Length(mv) > 0) {
                    mv = Vector3Normalize(mv);
                    float ty = atan2f(mv.x, mv.z); float dy = atan2f(sinf(ty - pyaw), cosf(ty - pyaw)); pyaw += dy * fminf(1, dt * 12);
                    Vector3 np = Vector3Add(player, Vector3Scale(mv, spd * dt));
                    if (!blocked(np, 0.4f)) player = np;
                    walkPhase += dt * spd * 2.4f; moving = fminf(1, moving + dt * 6);
                } else moving = fmaxf(0, moving - dt * 6);
                bool onWalk = false;
                float ax = fabsf(fmodf(fabsf(player.x), P) - P / 2), az = fabsf(fmodf(fabsf(player.z), P) - P / 2);
                onWalk = !(ax < ROAD / 2 || az < ROAD / 2) || player.x > beachX;
                player.y = onWalk && player.x < beachX ? 0.15f : 0;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {   // soco
                    for (auto& p : peds) if (!p.dead && Vector3Distance(p.pos, player) < 1.6f) { p.dead = true; money += (int)frand(5, 60); if (wanted < 1) wanted = 1; wantedTimer = 0; }
                }
            }
            // IA do trânsito
            for (auto& c : cars) {
                if (c.stun > 0) { c.stun -= dt; if (c.stun > 100) continue; if (c.stun > 0) continue; c.speed = frand(9, 15); }
                if (c.police) {
                    Vector3 to = Vector3Subtract(player, c.pos); float d = Vector3Length(to);
                    float ty = atan2f(to.x, to.z); float dy = atan2f(sinf(ty - c.yaw), cosf(ty - c.yaw));
                    c.yaw += Clamp(dy, -1, 1) * dt * 2.5f;
                    c.speed = Lerp(c.speed, d > 8 ? 34 : 4, dt);
                    if (d < 3.2f && (!inCar || fabsf(playerCar.speed) < 3)) { health -= dt * 25; }
                } else {
                    // segue a faixa e vira em cruzamentos
                    float fx = sinf(c.yaw), fz = cosf(c.yaw);
                    Vector3 ahead = {c.pos.x + fx * 6, 0, c.pos.z + fz * 6};
                    float slow = 1;
                    if (inCar && Vector3Distance(ahead, playerCar.pos) < 4) slow = 0;
                    for (auto& p : peds) if (!p.dead && Vector3Distance(ahead, p.pos) < 3) slow = 0;
                    c.speed = Lerp(c.speed, 13 * slow, dt * 2);
                    if (c.pos.x > beachX - 30 || c.pos.x < -440 || fabsf(c.pos.z) > 440) c.yaw += PI;
                }
                c.pos.x += sinf(c.yaw) * c.speed * dt; c.pos.z += cosf(c.yaw) * c.speed * dt;
            }
            for (auto& p : peds) {
                if (p.dead) continue;
                p.pos.x += sinf(p.yaw) * p.speed * dt; p.pos.z += cosf(p.yaw) * p.speed * dt; p.phase += dt * p.speed * 2.6f;
                if (fabsf(p.pos.x) > 430 || fabsf(p.pos.z) > 430 || p.pos.x > beachX - 30) p.yaw += PI;
            }
            // procurado
            int policeCount = 0; for (auto& c : cars) policeCount += c.police;
            if (wanted > 0 && policeCount < wanted * 2) {
                Car pc = spawnCar(true); float a = frand(0, 2 * PI);
                pc.pos = {player.x + cosf(a) * 120, 0, player.z + sinf(a) * 120}; pc.col = BLACK; pc.speed = 20;
                cars.push_back(pc);
            }
            wantedTimer += dt;
            if (wanted > 0 && wantedTimer > 25) { wanted--; wantedTimer = 0; if (wanted == 0) for (auto it = cars.begin(); it != cars.end();) it = it->police ? cars.erase(it) : it + 1; }
            if (health <= 0) { screen = BUSTED; EnableCursor(); }

            // câmera em 3ª pessoa
            Vector3 focus = inCar ? Vector3Add(playerCar.pos, {0, 1.6f, 0}) : Vector3Add(player, {0, 1.7f, 0});
            if (inCar && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && fabsf(md.x) < 0.5f) {
                float target = playerCar.yaw + PI; float dy = atan2f(sinf(target - camYaw), cosf(target - camYaw));
                camYaw += dy * fminf(1, dt * 2.5f);
            }
            float dist = inCar ? 8.5f + fabsf(playerCar.speed) * 0.06f : 3.6f;
            cam.fovy = inCar ? 55 + fabsf(playerCar.speed) * 0.25f : 50;
            Vector3 off = {sinf(camYaw) * cosf(camPitch) * dist, sinf(camPitch) * dist + 0.4f, cosf(camYaw) * cosf(camPitch) * dist};
            Vector3 sideOff = inCar ? Vector3{0, 0, 0} : Vector3{cosf(camYaw) * 0.6f, 0, -sinf(camYaw) * 0.6f};
            cam.target = Vector3Add(focus, sideOff);
            cam.position = Vector3Lerp(cam.position, Vector3Add(cam.target, off), fminf(1, dt * 12));
            if (cam.position.y < 0.4f) cam.position.y = 0.4f;
            if (shake > 0) { shake -= dt; cam.position = Vector3Add(cam.position, {frand(-1, 1) * shake * 0.3f, frand(-1, 1) * shake * 0.3f, 0}); }
        }

        // =============================== Iluminação ===============================
        float sunA = (timeOfDay - 6) / 12 * PI;
        Vector3 sunDir = Vector3Normalize({cosf(sunA) * 0.85f, sinf(sunA), 0.4f});
        float sunH = sunDir.y;
        float day = Clamp(sunH * 4 + 0.2f, 0, 1), dusk = Clamp(1 - fabsf(sunH) * 4, 0, 1);
        float night = 1 - Clamp(sunH * 6 + 0.5f, 0, 1);
        Vector3 lightDir = sunH > -0.05f ? sunDir : Vector3Normalize({-sunDir.x, -sunDir.y, 0.4f});
        Vector3 sunCol = Vector3Lerp({3.2f, 1.3f, 0.55f}, {3.0f, 2.85f, 2.6f}, Clamp(sunH * 3, 0, 1));
        Vector3 lightCol = sunH > -0.05f ? Vector3Scale(sunCol, Clamp(sunH * 8, 0, 1)) : Vector3{0.12f, 0.16f, 0.3f};
        Vector3 zen = Vector3Lerp({0.01f, 0.015f, 0.05f}, {0.12f, 0.32f, 0.75f}, day);
        Vector3 hor = Vector3Lerp(Vector3Lerp({0.04f, 0.04f, 0.09f}, {0.65f, 0.8f, 0.95f}, day), {1.1f, 0.45f, 0.3f}, dusk * 0.8f);
        Vector3 ambS = Vector3Lerp({0.04f, 0.05f, 0.1f}, {0.35f, 0.45f, 0.6f}, day); ambS = Vector3Lerp(ambS, {0.35f, 0.22f, 0.3f}, dusk * 0.5f);
        Vector3 ambG = Vector3Scale(ambS, 0.35f);
        Vector3 fogC = Vector3Scale(hor, 0.9f);
        float fogD = 0.0019f + dusk * 0.0008f;

        SetShaderValue(lit, lLightDir, &lightDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(lit, lLightCol, &lightCol, SHADER_UNIFORM_VEC3);
        SetShaderValue(lit, lAmbS, &ambS, SHADER_UNIFORM_VEC3);
        SetShaderValue(lit, lAmbG, &ambG, SHADER_UNIFORM_VEC3);
        SetShaderValue(lit, lView, &cam.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(lit, lFog, &fogC, SHADER_UNIFORM_VEC3);
        SetShaderValue(lit, lFogD, &fogD, SHADER_UNIFORM_FLOAT);
        SetShaderValue(lit, lNight, &night, SHADER_UNIFORM_FLOAT);
        SetShaderValue(lit, lTime, &t, SHADER_UNIFORM_FLOAT);
        int sOn = shadowsOn; SetShaderValue(lit, lShOn, &sOn, SHADER_UNIFORM_INT);
        float headOn = (inCar && night > 0.3f && screen != TITLE) ? 1.f : 0.f;
        Vector3 hp = {playerCar.pos.x + sinf(playerCar.yaw) * 2.4f, 0.9f, playerCar.pos.z + cosf(playerCar.yaw) * 2.4f};
        Vector3 hd = Vector3Normalize({sinf(playerCar.yaw), -0.12f, cosf(playerCar.yaw)});
        SetShaderValue(lit, lHeadP, &hp, SHADER_UNIFORM_VEC3);
        SetShaderValue(lit, lHeadD, &hd, SHADER_UNIFORM_VEC3);
        SetShaderValue(lit, lHeadOn, &headOn, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sky, sView, &cam.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(sky, sSun, &sunDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(sky, sSunC, &sunCol, SHADER_UNIFORM_VEC3);
        SetShaderValue(sky, sHor, &hor, SHADER_UNIFORM_VEC3);
        SetShaderValue(sky, sZen, &zen, SHADER_UNIFORM_VEC3);
        SetShaderValue(sky, sTime, &t, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sky, sNight, &night, SHADER_UNIFORM_FLOAT);

        Vector3 focusPt = screen == TITLE ? Vector3{0, 0, 0} : player;
        float cullR = screen == TITLE ? 600.f : 420.f;
        auto drawWorld = [&]() {
            if (!shadowPass) drawM(mPlane, TRS({0, 0, 0}, 0, {1000, 1, 1000}), WHITE, 0, 0.7f, 1);
            for (int x = -GRID / 2; x < GRID / 2; x++) for (int z = -GRID / 2; z < GRID / 2; z++) {
                float cx = x * P, cz = z * P;
                if (cx > beachX - P / 2) continue;
                if (Vector2Distance({cx, cz}, {focusPt.x, focusPt.z}) > cullR) continue;
                drawM(mCube, TRS({cx, 0.075f, cz}, 0, {BLOCK, 0.15f, BLOCK}), {150, 148, 140, 255}, 0, 0.85f);
                Vector3 lp = {cx + P / 2 - 3.f, 0, cz + P / 2 - 3.f};
                drawM(mCube, TRS({lp.x, 4, lp.z}, 0, {0.18f, 8, 0.18f}), {50, 52, 55, 255}, 0, 0.4f);
                drawM(mCube, TRS({cx + P / 2 + 4.f, 7.4f, cz + P / 2 + 4.f}, 0, {0.6f, 0.15f, 0.6f}), {255, 220, 170, 255}, night * 6, 0.2f);
            }
            for (auto& b : buildings) {
                if (Vector2Distance({b.pos.x, b.pos.z}, {focusPt.x, focusPt.z}) > cullR + 80) continue;
                drawM(mCube, TRS(b.pos, 0, b.size), b.col, 0, 0.75f, 2);
                drawM(mCube, TRS({b.pos.x, b.size.y + 0.4f, b.pos.z}, 0, {b.size.x + 0.4f, 0.8f, b.size.z + 0.4f}), {225, 222, 215, 255}, 0, 0.8f);
                if (b.size.y > 40) drawM(mCube, TRS({b.pos.x, b.size.y + 3.f, b.pos.z}, 0, {b.size.x * 0.4f, 5, b.size.z * 0.4f}), {120, 125, 130, 255}, 0, 0.5f);
            }
            for (auto& p : palms) {
                if (Vector2Distance({p.pos.x, p.pos.z}, {focusPt.x, focusPt.z}) > cullR) continue;
                Matrix lean = MatrixRotateZ(p.lean);
                for (int s = 0; s < 6; s++) {
                    float y0 = p.h * s / 6.f;
                    Vector3 o = Vector3Transform({0, y0 + p.h / 12, 0}, lean);
                    drawM(mCube, MatrixMultiply(MatrixMultiply(MatrixScale(0.42f - s * 0.03f, p.h / 6.f + 0.05f, 0.42f - s * 0.03f), lean), MatrixTranslate(p.pos.x + o.x, p.pos.y + o.y, p.pos.z + o.z)), {125, 100, 75, 255}, 0, 0.95f);
                }
                Vector3 top = Vector3Add(p.pos, Vector3Transform({0, p.h, 0}, lean));
                for (int i = 0; i < 9; i++) {
                    float a = i * 2 * PI / 9 + p.pos.x, sway = sinf(t * 1.3f + p.pos.z + i) * 0.06f;
                    Matrix fr = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixTranslate(0, 0, 2.2f), MatrixScale(0.9f, 0.08f, 1)), MatrixRotateX(0.55f + sway + (i % 2) * 0.2f)), MatrixRotateY(a));
                    fr = MatrixMultiply(MatrixMultiply(MatrixScale(1, 1, 1), fr), MatrixTranslate(top.x, top.y, top.z));
                    drawM(mCube, MatrixMultiply(MatrixScale(1, 1, 1), fr), {55, 120, 45, 255}, 0, 0.7f);
                }
            }
            for (auto& c : cars) if (Vector3Distance(c.pos, focusPt) < cullR) drawCar(c, t, night);
            if (!(screen == TITLE)) drawCar(playerCar, t, night);
            else drawCar(playerCar, t, night);
            for (auto& p : peds) {
                if (Vector3Distance(p.pos, focusPt) > 200) continue;
                if (p.dead) { drawM(mCube, TRS({p.pos.x, 0.3f, p.pos.z}, p.yaw, {0.5f, 0.3f, 1.7f}), p.shirt, 0, 0.8f); continue; }
                drawHuman(p.pos, p.yaw, p.phase, 1, p.shirt, {50, 60, 80, 255}, {200, 150, 115, 255});
            }
            if (!inCar && screen != TITLE) drawHuman(player, pyaw, walkPhase, moving, {240, 240, 235, 255}, {40, 45, 60, 255}, {190, 140, 105, 255});
        };

        // =============================== Sombra ===============================
        Matrix lightVP = MatrixIdentity();
        if (shadowsOn && lightDir.y > 0.02f) {
            Camera3D lc = {}; lc.up = {0, 1, 0}; lc.projection = CAMERA_ORTHOGRAPHIC; lc.fovy = screen == TITLE ? 700 : 220;
            Vector3 c = screen == TITLE ? Vector3{0, 0, 0} : Vector3{cam.target.x, 0, cam.target.z};
            lc.target = c; lc.position = Vector3Add(c, Vector3Scale(lightDir, 400));
            for (Model* m : all) m->materials[0].shader = depth;
            shadowPass = true;
            BeginTextureMode(shadowMap);
            ClearBackground(WHITE);
            rlSetClipPlanes(1, 1200);
            BeginMode3D(lc);
            lightVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
            drawWorld();
            EndMode3D();
            EndTextureMode();
            shadowPass = false;
            for (Model* m : all) m->materials[0].shader = lit;
        } else sOn = 0, SetShaderValue(lit, lShOn, &sOn, SHADER_UNIFORM_INT);
        SetShaderValueMatrix(lit, lLVP, lightVP);
        for (Model* m : all) m->materials[0].shader = lit;

        // =============================== Cena ===============================
        BeginTextureMode(rt);
        ClearBackground(BLACK);
        rlSetClipPlanes(0.1, 2000);
        BeginMode3D(cam);
        rlDisableBackfaceCulling(); rlDisableDepthMask();
        mSky.transform = MatrixMultiply(MatrixScale(1500, 1500, 1500), MatrixTranslate(cam.position.x, cam.position.y, cam.position.z));
        DrawModel(mSky, {0, 0, 0}, 1, WHITE);
        rlEnableDepthMask(); rlEnableBackfaceCulling();
        rlDrawRenderBatchActive();
        int slot = 10; rlActiveTextureSlot(slot); rlEnableTexture(shadowMap.depth.id); rlActiveTextureSlot(0);
        SetShaderValue(lit, lShadow, &slot, SHADER_UNIFORM_INT);
        drawWorld();
        EndMode3D();
        EndTextureMode();

        // =============================== Composição ===============================
        BeginDrawing();
        ClearBackground(BLACK);
        Vector2 res = {(float)rw, (float)rh}; float sharp = scale < 1 ? 0.6f : 0.15f, bl = bloomOn;
        SetShaderValue(post, pRes, &res, SHADER_UNIFORM_VEC2);
        SetShaderValue(post, pSharp, &sharp, SHADER_UNIFORM_FLOAT);
        SetShaderValue(post, pTime, &t, SHADER_UNIFORM_FLOAT);
        SetShaderValue(post, pBloom, &bl, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(post);
        DrawTexturePro(rt.texture, {0, 0, (float)rw, -(float)rh}, {0, 0, (float)W, (float)H}, {0, 0}, 0, WHITE);
        EndShaderMode();

        float u = H / 1080.f;
        auto txt = [&](const char* s, float x, float y, float sz, Color c) { DrawText(s, (int)(x * u + 2), (int)(y * u + 2), (int)(sz * u), Fade(BLACK, 0.6f)); DrawText(s, (int)(x * u), (int)(y * u), (int)(sz * u), c); };
        if (screen == TITLE || screen == SETTINGS) {
            DrawRectangleGradientH(0, 0, W / 2, H, Fade(BLACK, 0.75f), BLANK);
            DrawRectangleGradientV(0, H - 200 * u, W, 200 * u, BLANK, Fade({120, 0, 80, 255}, 0.5f));
            txt("NEON", 110, 150, 150, {255, 90, 170, 255});
            txt("COAST", 110, 290, 150, {90, 220, 240, 255});
            if (screen == TITLE) {
                const char* items[] = {"JOGAR", "CONFIGURACOES GRAFICAS", "SAIR"};
                for (int i = 0; i < 3; i++) txt(items[i], 120, 520 + i * 70, 44, i == menuSel ? Color{255, 210, 90, 255} : WHITE);
            } else {
                const char* lbl[] = {"Upscaling", "Sombras", "Bloom", "Hora do dia"};
                const char* val[] = {UPS_NAMES[upscaler], shadowsOn ? "Ultra (4096)" : "Desligadas", bloomOn ? "Ligado" : "Desligado", TextFormat("%02d:00", (int)timeOfDay)};
                for (int i = 0; i < 4; i++) txt(TextFormat("%s:  < %s >", lbl[i], val[i]), 120, 520 + i * 64, 36, i == setSel ? Color{255, 210, 90, 255} : WHITE);
                if (upscaler == UPS_DLSS) txt("DLSS exige o NVIDIA Streamline SDK e GPU RTX; nesta versao roda em resolucao nativa.", 120, 790, 24, ORANGE);
                txt("ENTER / -> alterar     ESC voltar", 120, 840, 26, Fade(WHITE, 0.75f));
            }
            txt("Setas: navegar   ENTER: selecionar", 120, 1010, 24, Fade(WHITE, 0.7f));
        } else {
            txt(TextFormat("$%d", money), 1620, 40, 54, {110, 230, 120, 255});
            for (int i = 0; i < 5; i++) {
                Vector2 c = {(1640 + i * 50.f) * u, 140 * u};
                DrawPoly(c, 5, 20 * u, -90, i < wanted ? Color{255, 255, 255, 255} : Fade(BLACK, 0.4f));
            }
            txt(TextFormat("%02d:%02d", (int)timeOfDay, (int)(fmodf(timeOfDay, 1) * 60)), 1640, 180, 34, WHITE);
            // minimapa
            Vector2 mc = {200 * u, H - 200 * u}; float mr = 150 * u, ms = 0.35f * u;
            DrawCircleV(mc, mr + 4 * u, Fade(BLACK, 0.7f)); DrawCircleV(mc, mr, {40, 60, 50, 230});
            float cy = cosf(camYaw), sy = sinf(camYaw);
            auto mp = [&](Vector3 w) { float dx = w.x - player.x, dz = w.z - player.z; return Vector2{mc.x + (dx * cy - dz * sy) * ms, mc.y + (dx * sy + dz * cy) * ms}; };
            for (auto& b : buildings) { Vector2 q = mp(b.pos); if (Vector2Distance(q, mc) < mr - 6 * u) DrawCircleV(q, fmaxf(b.size.x, b.size.z) * 0.32f * ms, {170, 170, 160, 255}); }
            Vector2 wq = mp({waterX, 0, player.z}); (void)wq;
            for (auto& c : cars) if (c.police) { Vector2 q = mp(c.pos); if (Vector2Distance(q, mc) < mr) DrawCircleV(q, 5 * u, fmodf(t * 4, 1) < 0.5f ? RED : BLUE); }
            if (!inCar) { Vector2 q = mp(playerCar.pos); if (Vector2Distance(q, mc) < mr) DrawCircleV(q, 6 * u, SKYBLUE); }
            DrawTriangle({mc.x, mc.y - 10 * u}, {mc.x - 7 * u, mc.y + 7 * u}, {mc.x + 7 * u, mc.y + 7 * u}, WHITE);
            DrawRectangle((int)(50 * u), (int)(H - 30 * u), (int)(300 * u), (int)(10 * u), Fade(BLACK, 0.6f));
            DrawRectangle((int)(50 * u), (int)(H - 30 * u), (int)(300 * u * health / 100), (int)(10 * u), {90, 200, 90, 255});
            if (inCar) txt(TextFormat("%d km/h", (int)fabsf(playerCar.speed * 3.6f)), 1620, 980, 48, WHITE);
            else {
                bool near = Vector3Distance(player, playerCar.pos) < 4.5f;
                for (auto& c : cars) near |= (!c.police && Vector3Distance(player, c.pos) < 4.f);
                if (near) txt("Pressione E para entrar no veiculo", 760, 900, 30, WHITE);
            }
            txt(TextFormat("%d FPS", GetFPS()), 20, 16, 22, Fade(WHITE, 0.6f));
            if (screen == PAUSED) { DrawRectangle(0, 0, W, H, Fade(BLACK, 0.6f)); txt("PAUSADO", 760, 420, 90, WHITE); txt("ESC continuar   |   Q menu principal", 700, 540, 32, WHITE); }
            if (screen == BUSTED) { DrawRectangle(0, 0, W, H, Fade({60, 0, 0, 255}, 0.55f)); txt("PRESO", 800, 420, 110, {255, 210, 90, 255}); txt("ENTER para continuar (perde metade do dinheiro)", 600, 560, 32, WHITE); }
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
