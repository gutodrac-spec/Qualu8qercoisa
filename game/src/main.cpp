// Neon Coast - jogo de mundo aberto em uma cidade costeira tropical (C++ / raylib / GLSL 330)
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <cstring>
#include <string>

// =====================================================================================
// Shaders
// =====================================================================================
static const char* LIT_VS = R"(#version 330
in vec3 vertexPosition; in vec3 vertexNormal; in vec4 vertexColor; in vec2 vertexTexCoord;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;
out vec3 fragPos; out vec3 fragNormal; out vec4 vcol; out vec2 uv0;
void main(){
    uv0 = vertexTexCoord;
    fragPos = (matModel*vec4(vertexPosition,1.0)).xyz;
    fragNormal = normalize((matNormal*vec4(vertexNormal,0.0)).xyz);
    vcol = vertexColor;
    gl_Position = mvp*vec4(vertexPosition,1.0);
})";

static const char* LIT_FS = R"(#version 330
in vec3 fragPos; in vec3 fragNormal; in vec4 vcol; in vec2 uv0;
uniform vec4 colDiffuse; uniform sampler2D texture0;
uniform vec3 lightDir, lightColor, ambSky, ambGround, viewPos, fogColor;
uniform float fogDensity, night, time, emissive, roughness;
uniform int surface;            // 0 generico, 1 terreno, 2 fachada, 3 calcada, 4 pintura automotiva
uniform mat4 lightVP; uniform sampler2D shadowMap; uniform int shadowsOn;
uniform float blockP, roadW, beachX, waterX;
uniform vec3 headPos, headDir; uniform float headOn;
uniform sampler2D tAsphD, tAsphN, tPaveD, tPaveN, tSandD, tSandN, tWallD, tWallN;
out vec4 finalColor;

float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898,78.233)))*43758.5453); }
float noise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y); }
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }
vec3 srgb(vec3 c){ return pow(c, vec3(2.2)); }
// mapeamento planar com normal map no espaco do mundo
vec3 planarN(sampler2D N, vec2 uv, vec3 T, vec3 B, vec3 n, float k){
    vec3 m = texture(N, uv).rgb*2.0-1.0; m.xy *= k;
    return normalize(T*m.x + B*m.y + n*m.z);
}
// quebra a repeticao da textura misturando duas escalas
vec3 detail(sampler2D D, vec2 uv){
    vec3 a = texture(D, uv).rgb, b = texture(D, uv*0.31+vec2(0.37,0.11)).rgb;
    return srgb(mix(a, a*b*1.9, 0.35)) * (0.85+0.3*noise(uv*0.37));
}

float shadow(vec3 n){
    if (shadowsOn==0) return 1.0;
    vec4 lp = lightVP*vec4(fragPos,1.0); vec3 p = lp.xyz/lp.w*0.5+0.5;
    if (p.x<0.0||p.x>1.0||p.y<0.0||p.y>1.0||p.z>1.0) return 1.0;
    float bias = max(0.0025*(1.0-dot(n,lightDir)), 0.0006);
    vec2 texel = 1.0/vec2(textureSize(shadowMap,0)); float s=0.0;
    float ang = hash(fragPos.xz*7.0)*6.283;
    mat2 rot = mat2(cos(ang),-sin(ang),sin(ang),cos(ang));
    for(int x=-2;x<=2;x++) for(int y=-2;y<=2;y++)
        s += (p.z-bias > texture(shadowMap, p.xy+rot*vec2(x,y)*texel*1.2).r) ? 0.0 : 1.0;
    float edge = smoothstep(0.0,0.06,min(min(p.x,1.0-p.x),min(p.y,1.0-p.y)));
    return mix(1.0, s/25.0, edge);
}

void main(){
    vec3 n = normalize(fragNormal);
    vec4 tex = texture(texture0, uv0);
    if (tex.a < 0.4) discard;
    vec3 albedo = srgb(colDiffuse.rgb * tex.rgb) * vcol.rgb;
    vec3 V = normalize(viewPos-fragPos);
    float rough = roughness, metal = 0.0, ao = 1.0; vec3 emis = vec3(0.0);
    bool water = false;
    vec3 Tx = vec3(1,0,0), Tz = vec3(0,0,1), Ty = vec3(0,1,0);

    if (surface==4){
        if (vcol.a < 0.5){ albedo = vec3(0.01,0.012,0.016); rough = 0.02; metal = 0.0; }
        else { rough = 0.18; metal = 0.25; }
        ao = mix(0.55, 1.0, smoothstep(0.25, 0.75, fragPos.y));
    } else if (surface==1){
        float x = fragPos.x, z = fragPos.z;
        if (x > waterX){
            water = true;
            float t = time; vec2 w = fragPos.xz;
            float dx = cos(w.x*0.35+t*1.3)*0.08 + cos(w.x*1.3+w.y*0.7+t*2.1)*0.04 + (noise(w*0.8+t)-0.5)*0.1 + (noise(w*3.0+t*1.7)-0.5)*0.05;
            float dz = cos(w.y*0.4+t*1.1)*0.08 + cos(w.y*1.7-w.x*0.5+t*1.7)*0.04 + (noise(w*0.8-t)-0.5)*0.1 + (noise(w*3.0-t*1.3)-0.5)*0.05;
            n = normalize(vec3(-dx,1.0,-dz));
            float shore = clamp((x-waterX)/50.0,0.0,1.0);
            albedo = mix(vec3(0.04,0.42,0.38), vec3(0.005,0.06,0.12), shore);
            rough = 0.04;
            float foam = smoothstep(0.7,1.0, noise(vec2(z*0.25, x*0.6 - t*0.7))) * (1.0-smoothstep(0.0,7.0,x-waterX));
            albedo = mix(albedo, vec3(0.85), foam); rough = mix(rough, 0.8, foam);
        } else if (x > beachX){
            vec2 uv = fragPos.xz*0.25;
            float wet = smoothstep(waterX-10.0, waterX, x);
            albedo = detail(tSandD, uv) * mix(1.0, 0.45, wet);
            n = planarN(tSandN, uv, Tx, Tz, n, 1.0);
            rough = mix(0.95, 0.25, wet);
        } else {
            vec2 uv = fragPos.xz*0.18;
            albedo = detail(tAsphD, uv) * 0.8;
            n = planarN(tAsphN, uv, Tx, Tz, n, 0.8);
            rough = 0.75 - 0.35*smoothstep(0.55,0.8,noise(fragPos.xz*0.08)); // pocas / partes gastas
            float ax = abs(mod(x, blockP)-blockP*0.5), az = abs(mod(z, blockP)-blockP*0.5);
            bool roadX = az < roadW*0.5, roadZ = ax < roadW*0.5;
            float wear = 0.6 + 0.4*noise(fragPos.xz*1.7);
            vec3 yel = vec3(0.55,0.38,0.03)*wear, wht = vec3(0.6)*wear;
            float m = 0.0; vec3 mc = wht;
            if (roadZ && !roadX && ax<0.12 && fract(z/6.0)<0.55) { m = 1.0; mc = yel; }
            if (roadX && !roadZ && az<0.12 && fract(x/6.0)<0.55) { m = 1.0; mc = yel; }
            if (roadZ && !roadX && abs(ax-roadW*0.5+0.6)<0.08) m = 1.0;
            if (roadX && !roadZ && abs(az-roadW*0.5+0.6)<0.08) m = 1.0;
            if (roadX && roadZ && (fract((x+z)*0.5)<0.5) && (ax>roadW*0.35 || az>roadW*0.35)) m = 1.0;
            albedo = mix(albedo, mc, m); rough = mix(rough, 0.6, m);
            // oclusao junto ao meio-fio
            float curb = min(roadZ ? roadW*0.5-ax : 99.0, roadX ? roadW*0.5-az : 99.0);
            if (!(roadX && roadZ)) ao *= mix(0.55, 1.0, smoothstep(0.0, 0.8, curb));
        }
    } else if (surface==3){
        if (abs(n.y) > 0.5){
            vec2 uv = fragPos.xz*0.3;
            albedo = detail(tPaveD, uv);
            n = planarN(tPaveN, uv, Tx, Tz, n, 1.0);
        } else albedo *= 0.8;
        rough = 0.85;
    } else if (surface==2 && abs(n.y)<0.5){
        bool fx = abs(n.x)>abs(n.z);
        float u = fx ? fragPos.z : fragPos.x;
        vec2 wuv = vec2(u, fragPos.y)*0.25;
        vec3 wall = detail(tWallD, wuv);
        vec3 baseN = n;
        n = planarN(tWallN, wuv, fx ? Tz : Tx, Ty, n, 1.0);
        albedo *= wall*1.6;
        vec2 cell = floor(vec2(u/3.0, fragPos.y/3.6));
        vec2 f = fract(vec2(u/3.0, fragPos.y/3.6));
        float win = step(0.18,f.x)*step(f.x,0.82)*step(0.22,f.y)*step(f.y,0.85)*step(3.0,fragPos.y);
        float frame = (step(0.14,f.x)*step(f.x,0.86)*step(0.18,f.y)*step(f.y,0.89)) - win;
        float r = hash(cell + floor(fragPos.xz/7.0)*13.1);
        albedo = mix(albedo, vec3(0.6), frame*0.8);
        // vidro: reflete o ceu, cortinas aleatorias
        vec3 glass = mix(vec3(0.02,0.03,0.04), vec3(0.25,0.22,0.18), step(0.7, hash(cell*3.7))*0.5);
        albedo = mix(albedo, glass, win);
        if (win > 0.5) n = baseN;
        rough = mix(rough, 0.03, win); metal = win*0.2;
        ao *= mix(1.0, 0.75, frame) * mix(0.7, 1.0, smoothstep(0.0, 4.0, fragPos.y));
        float interior = step(0.5, r) * (0.6+0.4*hash(cell+1.7));
        emis += win * interior * night * mix(vec3(1.0,0.68,0.32), vec3(0.75,0.85,1.0), step(0.85,r)) * 1.8;
    }

    vec3 L = normalize(lightDir), H = normalize(L+V);
    float NdL = max(dot(n,L),0.0);
    float sh = shadow(normalize(fragNormal));
    float a = max(rough*rough, 0.002), NdH = max(dot(n,H),0.0);
    float D = a*a / (3.14159 * pow(NdH*NdH*(a*a-1.0)+1.0, 2.0));         // GGX
    float NdV = max(dot(n,V),0.001);
    float k = (rough+1.0)*(rough+1.0)/8.0;
    float G = (NdV/(NdV*(1.0-k)+k)) * (NdL/(NdL*(1.0-k)+k));
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    vec3 F = F0 + (1.0-F0)*pow(1.0-max(dot(H,V),0.0), 5.0);
    vec3 specB = D*G*F / max(4.0*NdV*NdL, 0.001);
    vec3 kd = (1.0-F)*(1.0-metal);

    vec3 col = (kd*albedo/3.14159*3.14159 + specB) * lightColor * NdL * sh;
    vec3 amb = mix(ambGround, ambSky, n.y*0.5+0.5) * albedo * ao * (1.0-metal*0.5);
    col += amb;
    vec3 R = reflect(-V,n);
    vec3 Fe = F0 + (max(vec3(1.0-rough), F0)-F0)*pow(1.0-NdV, 5.0);
    float sunR = pow(max(dot(R,L),0.0), 300.0) * (1.0-rough) * 30.0;
    vec3 env = mix(fogColor*1.1, ambSky*2.4, smoothstep(-0.05, 0.6, R.y));
    if (R.y < 0.0) env = mix(env, ambGround*0.6, 0.6);
    col += (env + lightColor*sunR*sh) * Fe * (1.0-rough*0.9) * ao * (water?1.3:1.0);

    // postes de luz procedurais nos cruzamentos
    vec2 c = (floor(fragPos.xz/blockP)+0.5)*blockP;
    vec3 lamp = vec3(c.x+4.0, 7.3, c.y+4.0);
    vec3 dl = lamp-fragPos; float d2 = dot(dl,dl);
    col += albedo * vec3(1.0,0.6,0.25) * 140.0 / (d2+6.0) * max(dot(n,normalize(dl)),0.0) * night;
    if (water || rough < 0.3) col += vec3(1.0,0.6,0.25) * pow(max(dot(R, normalize(dl)),0.0), 80.0) * night * 3.0;
    if (headOn>0.5){
        vec3 hd = fragPos-headPos; float hl = length(hd);
        float cone = smoothstep(0.82,0.95, dot(hd/hl, headDir));
        col += albedo * vec3(1.0,0.95,0.85) * cone * 140.0/(hl*hl+6.0) * max(dot(n,-hd/hl),0.0);
    }

    col += emis + albedo*emissive;
    float dist = length(viewPos-fragPos);
    float fog = 1.0 - exp(-pow(dist*fogDensity, 1.5));
    fog *= mix(1.0, 0.6, smoothstep(0.0, 80.0, fragPos.y));      // neblina por altura
    col = mix(col, fogColor, clamp(fog,0.0,1.0));
    col = aces(col*1.15);
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

// Pós-processamento: FXAA, nitidez (upscale), bloom, aberração cromática, vinheta, grão
static const char* POST_FS = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; uniform vec2 srcRes; uniform float sharpen, time, bloomOn;
out vec4 finalColor;
float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898,78.233)))*43758.5453); }
float luma(vec3 c){ return dot(c, vec3(0.299,0.587,0.114)); }
vec3 fxaa(vec2 uv, vec2 px){
    vec3 rgbM = texture(texture0, uv).rgb;
    float lNW = luma(texture(texture0, uv+vec2(-1,-1)*px).rgb), lNE = luma(texture(texture0, uv+vec2(1,-1)*px).rgb);
    float lSW = luma(texture(texture0, uv+vec2(-1,1)*px).rgb), lSE = luma(texture(texture0, uv+vec2(1,1)*px).rgb);
    float lM = luma(rgbM);
    float lMin = min(lM, min(min(lNW,lNE),min(lSW,lSE))), lMax = max(lM, max(max(lNW,lNE),max(lSW,lSE)));
    vec2 dir = vec2(-((lNW+lNE)-(lSW+lSE)), ((lNW+lSW)-(lNE+lSE)));
    float red = max((lNW+lNE+lSW+lSE)*0.03125, 1.0/128.0);
    float rcp = 1.0/(min(abs(dir.x),abs(dir.y))+red);
    dir = clamp(dir*rcp, -8.0, 8.0)*px;
    vec3 A = 0.5*(texture(texture0, uv+dir*(1.0/3.0-0.5)).rgb + texture(texture0, uv+dir*(2.0/3.0-0.5)).rgb);
    vec3 B = A*0.5 + 0.25*(texture(texture0, uv-dir*0.5).rgb + texture(texture0, uv+dir*0.5).rgb);
    float lB = luma(B);
    return (lB<lMin || lB>lMax) ? A : B;
}
void main(){
    vec2 uv = fragTexCoord; vec2 px = 1.0/srcRes;
    vec2 cc = uv-0.5;
    vec3 c = fxaa(uv, px);
    c.r = mix(c.r, texture(texture0, uv - cc*0.003).r, 0.6);
    c.b = mix(c.b, texture(texture0, uv + cc*0.003).b, 0.6);
    if (sharpen>0.0){
        vec3 nb = texture(texture0,uv+vec2(px.x,0)).rgb + texture(texture0,uv-vec2(px.x,0)).rgb
                + texture(texture0,uv+vec2(0,px.y)).rgb + texture(texture0,uv-vec2(0,px.y)).rgb;
        c = clamp(c + (c*4.0-nb)*sharpen*0.2, 0.0, 1.0);
    }
    if (bloomOn>0.5){
        vec3 b = vec3(0.0); float wsum = 0.0;
        for(int i=0;i<24;i++){
            float a = float(i)*2.39996; float r = 3.0 + float(i)*1.4; float w = 1.0/(1.0+float(i)*0.15);
            vec3 s = texture(texture0, uv+vec2(cos(a),sin(a))*px*r).rgb;
            b += max(s-0.65,0.0)*w; wsum += w;
        }
        c += b/wsum*2.2*vec3(1.0,0.92,0.85);
    }
    // grading: sombras levemente azul-petroleo, altas luzes quentes (look cinematografico)
    float l = luma(c);
    c = mix(c, c*vec3(0.92,1.0,1.06), (1.0-l)*0.35);
    c = mix(c, c*vec3(1.06,1.0,0.92), l*0.3);
    c = mix(vec3(l), c, 1.12);
    c *= 1.0 - dot(cc,cc)*0.85;
    c += (hash(uv*srcRes+time)-0.5)*0.02;
    finalColor = vec4(c,1.0);
})";


// =====================================================================================
// Mundo
// =====================================================================================
struct Building { Vector3 pos, size; Color col; int style; Color neon; };
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

// =====================================================================================
// Geração procedural de malhas
// =====================================================================================
struct MeshBuilder {
    std::vector<float> v, n; std::vector<unsigned char> c;
    void vert(Vector3 p, Vector3 nn, Color col) {
        v.insert(v.end(), {p.x, p.y, p.z}); n.insert(n.end(), {nn.x, nn.y, nn.z}); c.insert(c.end(), {col.r, col.g, col.b, col.a});
    }
    void tri(Vector3 a, Vector3 b, Vector3 d, Color col) {
        Vector3 nn = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
        vert(a, nn, col); vert(b, nn, col); vert(d, nn, col);
    }
    Mesh build() {
        Mesh m = {0};
        m.vertexCount = (int)v.size() / 3; m.triangleCount = m.vertexCount / 3;
        m.vertices = (float*)MemAlloc(v.size() * sizeof(float)); memcpy(m.vertices, v.data(), v.size() * sizeof(float));
        m.normals = (float*)MemAlloc(n.size() * sizeof(float)); memcpy(m.normals, n.data(), n.size() * sizeof(float));
        m.colors = (unsigned char*)MemAlloc(c.size()); memcpy(m.colors, c.data(), c.size());
        UploadMesh(&m, false);
        return m;
    }
};

static float keyInterp(const float (*k)[2], int count, float u) {
    for (int i = 0; i < count - 1; i++) if (u <= k[i + 1][0]) {
        float t = (u - k[i][0]) / (k[i + 1][0] - k[i][0]); t = t * t * (3 - 2 * t);
        return k[i][1] + (k[i + 1][1] - k[i][1]) * t;
    }
    return k[count - 1][1];
}

// Carroceria de sedan: seções transversais superelípticas ao longo do comprimento, com vidros (alpha 0)
static Mesh GenCarBody(float L, bool sporty) {
    const int NS = 64, NR = 40;
    static const float topK[][2] = {{0, 0.62f}, {0.03f, 0.92f}, {0.2f, 1.0f}, {0.36f, 1.42f}, {0.6f, 1.44f}, {0.76f, 1.03f}, {0.95f, 0.93f}, {1.0f, 0.6f}};
    static const float topS[][2] = {{0, 0.6f}, {0.03f, 0.88f}, {0.16f, 0.95f}, {0.38f, 1.26f}, {0.56f, 1.27f}, {0.72f, 0.92f}, {0.95f, 0.84f}, {1.0f, 0.55f}};
    std::vector<Vector3> P((NS + 1) * NR); std::vector<unsigned char> glass((NS + 1) * NR);
    for (int i = 0; i <= NS; i++) {
        float u = i / (float)NS, z = (u - 0.5f) * L;
        float yt = sporty ? keyInterp(topS, 8, u) : keyInterp(topK, 8, u), yb = 0.3f;
        float end = fminf(u, 1 - u); float endS = sqrtf(fminf(1.f, end / 0.035f));
        float a = 0.92f * (1 - powf(fabsf(2 * u - 1), 8) * 0.18f) * (0.55f + 0.45f * endS);
        bool cabin = yt > 1.1f || (sporty && yt > 1.0f);
        float belt = sporty ? 0.9f : 1.0f;
        for (int k = 0; k < NR; k++) {
            float th = 2 * PI * k / NR, cs = cosf(th), sn = sinf(th);
            float cx = copysignf(powf(fabsf(cs), 2.f / 5), cs), cy = copysignf(powf(fabsf(sn), 2.f / 5), sn);
            float y = yb + (yt - yb) * (0.5f + 0.5f * cy);
            float tumble = 1 - 0.3f * Clamp((y - belt) / fmaxf(yt - belt, 0.01f), 0, 1) * (cabin ? 1 : 0);
            float x = a * cx * tumble;
            if (y < 0.45f) x *= 0.94f;
            P[i * NR + k] = {x, (y - yb) * endS + yb + (1 - endS) * 0.15f, z};
            glass[i * NR + k] = (cabin && y > belt + 0.04f && y < yt - 0.06f) ? 1 : 0;
        }
    }
    MeshBuilder mb;
    auto at = [&](int i, int k) { return P[i * NR + (k + NR) % NR]; };
    std::vector<Vector3> N(P.size());
    for (int i = 0; i <= NS; i++) for (int k = 0; k < NR; k++) {
        Vector3 du = Vector3Subtract(at(i < NS ? i + 1 : i, k), at(i > 0 ? i - 1 : i, k));
        Vector3 dv = Vector3Subtract(at(i, k + 1), at(i, k - 1));
        N[i * NR + k] = Vector3Normalize(Vector3CrossProduct(dv, du));
    }
    for (int i = 0; i < NS; i++) for (int k = 0; k < NR; k++) {
        int a0 = i * NR + k, a1 = i * NR + (k + 1) % NR, b0 = (i + 1) * NR + k, b1 = (i + 1) * NR + (k + 1) % NR;
        bool g = glass[a0] && glass[a1] && glass[b0] && glass[b1];
        Color col = g ? Color{255, 255, 255, 0} : WHITE;
        mb.vert(P[a0], N[a0], col); mb.vert(P[b1], N[b1], col); mb.vert(P[b0], N[b0], col);
        mb.vert(P[a0], N[a0], col); mb.vert(P[a1], N[a1], col); mb.vert(P[b1], N[b1], col);
    }
    for (int e = 0; e < 2; e++) {   // tampas
        int i = e ? NS : 0; Vector3 c = {0, 0.5f, P[i * NR].z}; Vector3 nn = {0, 0, e ? 1.f : -1.f};
        for (int k = 0; k < NR; k++) {
            Vector3 p0 = at(i, k), p1 = at(i, k + 1);
            if (e) { mb.vert(c, nn, WHITE); mb.vert(p1, nn, WHITE); mb.vert(p0, nn, WHITE); }
            else { mb.vert(c, nn, WHITE); mb.vert(p0, nn, WHITE); mb.vert(p1, nn, WHITE); }
        }
    }
    return mb.build();
}

// Folha de palmeira: nervura curva com folíolos dos dois lados
static Mesh GenFrond() {
    MeshBuilder mb; const int S = 14;
    auto spine = [](float s) { return Vector3{0, 0.7f * s - 1.9f * s * s, 4.2f * s}; };
    for (int i = 0; i < S; i++) {
        float s0 = i / (float)S, s1 = (i + 1) / (float)S;
        Vector3 p0 = spine(s0), p1 = spine(s1);
        for (int side = -1; side <= 1; side += 2) {
            float l0 = 1.15f * sinf(PI * fminf(1, s0 * 1.1f + 0.05f)), l1 = 1.15f * sinf(PI * fminf(1, s1 * 1.1f + 0.05f));
            Vector3 t0 = Vector3Add(p0, {side * l0, -0.35f * l0, 0.45f * l0}), t1 = Vector3Add(p1, {side * l1, -0.35f * l1, 0.45f * l1});
            Color g0 = {(unsigned char)(70 + 40 * s0), (unsigned char)(130 + 30 * s0), 50, 255};
            mb.tri(p0, p1, t0, g0); mb.tri(p1, t1, t0, g0);
            mb.tri(p0, t0, p1, g0); mb.tri(p1, t0, t1, g0);   // dupla face
        }
    }
    return mb.build();
}

// =====================================================================================
// Modelos externos hiper-realistas (.glb/.gltf de Sketchfab, Fab, etc.) em models/
// Se o arquivo existir ele substitui o modelo procedural correspondente.
// =====================================================================================
struct Ext { Model m = {0}; bool ok = false; Matrix fix = MatrixIdentity(); ModelAnimation* anims = nullptr; int animCount = 0, walk = -1, idle = -1, run = -1; };
static Ext extPolice, extPlayer, extPalm;
static std::vector<Ext> extCars, extPeds;

static Ext LoadExt(const char* name, float target, bool byLength) {
    Ext e;
    std::string dir = std::string(GetApplicationDirectory()) + "models/", path;
    for (const char* ext : {".glb", ".gltf"}) if (FileExists((dir + name + ext).c_str())) { path = dir + name + ext; break; }
    if (path.empty()) return e;
    e.m = LoadModel(path.c_str());
    if (e.m.meshCount == 0) return e;
    for (int i = 0; i < e.m.materialCount; i++) e.m.materials[i].shader = lit;
    e.anims = LoadModelAnimations(path.c_str(), &e.animCount);
    for (int i = 0; i < e.animCount; i++) {
        if (!IsModelAnimationValid(e.m, e.anims[i])) continue;
        std::string n = TextToLower(e.anims[i].name);
        if (n.find("walk") != std::string::npos && e.walk < 0) e.walk = i;
        else if (n.find("run") != std::string::npos && e.run < 0) e.run = i;
        else if (n.find("idle") != std::string::npos && e.idle < 0) e.idle = i;
    }
    if (e.walk < 0 && e.animCount > 0 && IsModelAnimationValid(e.m, e.anims[0])) e.walk = 0;
    // mede já na pose animada (esqueletos do Mixamo usam escala 0.01)
    int ref0 = e.idle >= 0 ? e.idle : e.walk;
    if (ref0 >= 0) UpdateModelAnimation(e.m, e.anims[ref0], 0);
    BoundingBox bb = {{1e9f, 1e9f, 1e9f}, {-1e9f, -1e9f, -1e9f}};
    for (int i = 0; i < e.m.meshCount; i++) {
        Mesh& ms = e.m.meshes[i]; float* vv = (ref0 >= 0 && ms.animVertices) ? ms.animVertices : ms.vertices;
        for (int k = 0; k < ms.vertexCount; k++) { Vector3 q = {vv[k * 3], vv[k * 3 + 1], vv[k * 3 + 2]}; bb.min = Vector3Min(bb.min, q); bb.max = Vector3Max(bb.max, q); }
    }
    Vector3 sz = Vector3Subtract(bb.max, bb.min);
    float yaw = 0;
    std::string yf = dir + name + ".yaw";   // arquivo opcional com a rotação em graus para alinhar a frente com +Z
    if (FileExists(yf.c_str())) { char* s = LoadFileText(yf.c_str()); yaw = (float)atof(s) * DEG2RAD; UnloadFileText(s); }
    else if (byLength && sz.x > sz.z) yaw = PI / 2;
    float ref = byLength ? fmaxf(sz.x, sz.z) : sz.y;
    float sc = target / fmaxf(ref, 0.001f);
    Vector3 c = {(bb.min.x + bb.max.x) / 2, bb.min.y, (bb.min.z + bb.max.z) / 2};
    e.fix = MatrixMultiply(MatrixMultiply(MatrixTranslate(-c.x, -c.y, -c.z), MatrixScale(sc, sc, sc)), MatrixRotateY(yaw));
    e.ok = true;
    TraceLog(LOG_INFO, "Modelo externo carregado: %s (%d animacoes)", path.c_str(), e.animCount);
    return e;
}

static void LoadExternalModels() {
    for (int i = 1; i <= 8; i++) { Ext e = LoadExt(TextFormat("car%d", i), 4.6f, true); if (e.ok) extCars.push_back(e); }
    { Ext e = LoadExt("car", 4.6f, true); if (e.ok) extCars.push_back(e); }
    for (int i = 1; i <= 8; i++) { Ext e = LoadExt(TextFormat("pedestrian%d", i), 1.78f, false); if (e.ok) extPeds.push_back(e); }
    extPolice = LoadExt("police", 4.9f, true);
    extPlayer = LoadExt("player", 1.8f, false);
    extPalm = LoadExt("palm", 10.f, false);
}

static void SetExtShader(Shader s) {
    auto set = [&](Ext& e) { if (e.ok) for (int i = 0; i < e.m.materialCount; i++) e.m.materials[i].shader = s; };
    set(extPolice); set(extPlayer); set(extPalm);
    for (auto& e : extCars) set(e);
    for (auto& e : extPeds) set(e);
}

static void drawExt(Ext& e, Matrix world, int anim = -1, float animTime = 0, float rough = 0.5f) {
    if (anim >= 0 && anim < e.animCount && e.anims[anim].frameCount > 0)
        UpdateModelAnimation(e.m, e.anims[anim], (int)(animTime * 30) % e.anims[anim].frameCount);
    if (!shadowPass) { setUniF(locEmissive, 0); setUniF(locRough, rough); setUniI(locSurface, 0); }
    e.m.transform = MatrixMultiply(e.fix, world);
    DrawModel(e.m, {0, 0, 0}, 1.f, WHITE);
}

static Model mCarBody, mCarSport, mFrond, mRim;

static void drawCar(const Car& c, float t, float night) {
    Vector3 b = {c.pos.x, 0, c.pos.z};
    float L = 4.6f, W = 1.84f;
    Color body = c.police ? Color{235, 235, 240, 255} : c.col;
    Ext* ex = (c.police && extPolice.ok) ? &extPolice : (!c.police && !extCars.empty() && (c.col.r + c.col.b + c.type) % 5 < 3) ? &extCars[(c.col.r + c.col.g + c.type) % extCars.size()] : nullptr;
    if (ex) {
        drawExt(*ex, local(b, c.yaw, {0, 0, 0}, {1, 1, 1}), -1, 0, 0.25f);
        if (c.police) {
            bool ph = fmodf(t * 4, 1) < 0.5f;
            drawM(mCube, local(b, c.yaw, {-0.3f, 1.55f, -0.1f}, {0.5f, 0.12f, 0.22f}), {255, 30, 30, 255}, ph ? 10.f : 0.3f);
            drawM(mCube, local(b, c.yaw, {0.3f, 1.55f, -0.1f}, {0.5f, 0.12f, 0.22f}), {30, 60, 255, 255}, ph ? 0.3f : 10.f);
        }
        return;
    }
    drawM(c.type == 1 ? mCarSport : mCarBody, local(b, c.yaw, {0, 0, 0}, {1, 1, 1}), body, 0, 0.2f, 4);
    if (c.police) drawM(mCube, local(b, c.yaw, {0, 0.66f, 0.1f}, {W * 0.995f, 0.28f, 1.9f}), {15, 15, 20, 255}, 0, 0.25f);
    for (int i = 0; i < 4; i++) {
        float s = (i & 1) ? 1.f : -1.f, sz = (i & 2) ? L * 0.31f : -L * 0.3f;
        Matrix rot = MatrixMultiply(MatrixTranslate(0, -0.13f, 0), MatrixRotateZ(PI / 2));
        drawM(mCyl, local(b, c.yaw, {s * (W / 2 - 0.12f), 0.38f, sz}, {1, 1, 1}, rot), {18, 18, 18, 255}, 0, 0.85f);
        drawM(mRim, local(b, c.yaw, {s * (W / 2 - 0.1f), 0.38f, sz}, {1, 1, 1}, rot), {170, 172, 178, 255}, 0, 0.2f);
    }
    float hl = 0.4f + night * 4.f;
    for (int s = -1; s <= 1; s += 2) {
        drawM(mSphere, local(b, c.yaw, {s * W * 0.33f, 0.78f, L / 2 - 0.12f}, {0.22f, 0.07f, 0.1f}), {255, 250, 230, 255}, hl, 0.05f);
        drawM(mSphere, local(b, c.yaw, {s * W * 0.36f, 0.86f, -L / 2 + 0.1f}, {0.2f, 0.06f, 0.08f}), {255, 15, 15, 255}, 0.6f + night * 2.5f, 0.05f);
        drawM(mCube, local(b, c.yaw, {s * W * 0.47f, 1.0f, L * 0.12f}, {0.18f, 0.08f, 0.1f}), body, 0, 0.2f, 4);   // retrovisores
    }
    if (c.police) {
        bool ph = fmodf(t * 4, 1) < 0.5f;
        drawM(mCube, local(b, c.yaw, {-0.3f, 1.5f, -0.1f}, {0.5f, 0.12f, 0.22f}), {255, 30, 30, 255}, ph ? 10.f : 0.3f);
        drawM(mCube, local(b, c.yaw, {0.3f, 1.5f, -0.1f}, {0.5f, 0.12f, 0.22f}), {30, 60, 255, 255}, ph ? 0.3f : 10.f);
    }
}

static void drawHuman(Vector3 p, float yaw, float phase, float moving, Color shirt, Color pants, Color skin, Ext* ex = nullptr, bool animate = true) {
    if (ex && ex->ok) {
        int anim = moving > 0.3f ? (ex->walk >= 0 ? ex->walk : ex->idle) : (ex->idle >= 0 ? ex->idle : -1);
        drawExt(*ex, TRS(p, yaw, {1, 1, 1}), animate ? anim : -1, phase / 3.2f, 0.6f);
        return;
    }
    float sw = sinf(phase) * 0.55f * moving, bob = fabsf(sinf(phase)) * 0.045f * moving;
    Vector3 b = {p.x, p.y + bob, p.z};
    Color hair = {35, 24, 18, 255};
    drawM(mSphere, local(b, yaw, {0, 1.33f, 0}, {0.22f, 0.3f, 0.14f}), shirt, 0, 0.85f);              // torso
    drawM(mSphere, local(b, yaw, {0, 1.5f, 0}, {0.25f, 0.12f, 0.14f}), shirt, 0, 0.85f);              // ombros
    drawM(mSphere, local(b, yaw, {0, 0.98f, 0}, {0.19f, 0.13f, 0.13f}), pants, 0, 0.85f);             // quadril
    drawM(mSphere, local(b, yaw, {0, 1.63f, 0}, {0.055f, 0.08f, 0.055f}), skin, 0, 0.6f);             // pescoço
    drawM(mSphere, local(b, yaw, {0, 1.74f, 0.01f}, {0.1f, 0.125f, 0.115f}), skin, 0, 0.55f);         // cabeça
    drawM(mSphere, local(b, yaw, {0, 1.79f, -0.015f}, {0.105f, 0.095f, 0.115f}), hair, 0, 0.9f);      // cabelo
    for (int s = -1; s <= 1; s += 2) {
        Matrix hip = MatrixRotateX(sw * s);
        Matrix thigh = MatrixMultiply(MatrixTranslate(0, -0.22f, 0), hip);
        drawM(mSphere, local(b, yaw, {0.1f * s, 0.95f, 0}, {0.085f, 0.24f, 0.095f}, thigh), pants, 0, 0.85f);
        float knee = fmaxf(0, -sw * s) * 0.9f;
        Matrix shin = MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -0.22f, 0), MatrixRotateX(knee)), MatrixMultiply(MatrixTranslate(0, -0.44f, 0), hip));
        drawM(mSphere, local(b, yaw, {0.1f * s, 0.95f, 0}, {0.07f, 0.24f, 0.08f}, shin), pants, 0, 0.85f);
        Matrix foot = MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -0.47f, 0.06f), MatrixRotateX(knee)), MatrixMultiply(MatrixTranslate(0, -0.44f, 0), hip));
        drawM(mSphere, local(b, yaw, {0.1f * s, 0.95f, 0}, {0.06f, 0.045f, 0.13f}, foot), {30, 30, 32, 255}, 0, 0.5f);
        Matrix sh = MatrixMultiply(MatrixRotateZ(0.08f * s), MatrixRotateX(-sw * s * 0.8f));
        Matrix upper = MatrixMultiply(MatrixTranslate(0, -0.15f, 0), sh);
        drawM(mSphere, local(b, yaw, {0.27f * s, 1.5f, 0}, {0.06f, 0.16f, 0.065f}, upper), shirt, 0, 0.85f);
        Matrix fore = MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -0.14f, 0), MatrixRotateX(-0.3f - fmaxf(0, sw * s) * 0.6f)), MatrixMultiply(MatrixTranslate(0, -0.3f, 0), sh));
        drawM(mSphere, local(b, yaw, {0.27f * s, 1.5f, 0}, {0.048f, 0.15f, 0.05f}, fore), skin, 0, 0.6f);
    }
}

// =====================================================================================
// Texturas: carrega fotos PBR (assets/*.jpg, baixadas do Poly Haven no build) ou gera por código
// =====================================================================================
static float phash(int x, int y, int per) { x = ((x % per) + per) % per; y = ((y % per) + per) % per;
    unsigned h = (unsigned)(x * 374761393 + y * 668265263); h = (h ^ (h >> 13)) * 1274126177u; return (h ^ (h >> 16)) / 4294967295.f; }
static float pnoise(float x, float y, int per) {
    int ix = (int)floorf(x), iy = (int)floorf(y); float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    return Lerp(Lerp(phash(ix, iy, per), phash(ix + 1, iy, per), fx), Lerp(phash(ix, iy + 1, per), phash(ix + 1, iy + 1, per), fx), fy);
}
static float pfbm(float x, float y, int per) { float v = 0, a = 0.5f; for (int i = 0; i < 6; i++) { v += a * pnoise(x, y, per); x *= 2; y *= 2; per *= 2; a *= 0.5f; } return v; }

static void GenMaterialImages(int kind, Image& diff, Image& nor) {
    const int S = 512; std::vector<float> h(S * S);
    diff = GenImageColor(S, S, WHITE); nor = GenImageColor(S, S, WHITE);
    Color* d = (Color*)diff.data;
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
        float u = x / (float)S * 8, v = y / (float)S * 8, f = pfbm(u, v, 8), w = phash(x, y, S);
        float r, g, b, ht;
        if (kind == 0) { float k = 0.28f + 0.14f * f + 0.06f * (w > 0.96f ? w : 0) - 0.05f * pnoise(u * 0.5f, v * 0.5f, 4); r = k; g = k; b = k * 1.04f; ht = f * 0.6f + w * 0.4f; }
        else if (kind == 1) {
            float tu = fmodf(u * 0.5f, 1), tv = fmodf(v * 0.5f, 1); int tx = (int)(u * 0.5f), ty = (int)(v * 0.5f);
            float groove = fminf(fminf(tu, 1 - tu), fminf(tv, 1 - tv)); float gm = Clamp(groove / 0.025f, 0, 1);
            float k = (0.55f + 0.08f * phash(tx, ty, 4) + 0.1f * f) * (0.45f + 0.55f * gm); r = k; g = k * 0.98f; b = k * 0.95f; ht = gm * 0.8f + f * 0.2f;
        } else if (kind == 2) { float rip = sinf(u * 6 + f * 6) * 0.5f + 0.5f; float k = 0.9f + 0.12f * f + 0.03f * (w - 0.5f); r = 0.8f * k; g = 0.69f * k; b = 0.5f * k; ht = rip * 0.3f + w * 0.5f + f * 0.2f; }
        else { float k = 0.82f + 0.08f * f + 0.01f * (w - 0.5f); r = k; g = k * 0.985f; b = k * 0.96f; ht = f * 0.9f + w * 0.1f; }
        d[y * S + x] = {(unsigned char)(Clamp(r, 0, 1) * 255), (unsigned char)(Clamp(g, 0, 1) * 255), (unsigned char)(Clamp(b, 0, 1) * 255), 255};
        h[y * S + x] = ht;
    }
    Color* n = (Color*)nor.data; float str = kind == 1 ? 6.f : 3.f;
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
        float dx = h[y * S + (x + 1) % S] - h[y * S + (x + S - 1) % S], dy = h[((y + 1) % S) * S + x] - h[((y + S - 1) % S) * S + x];
        Vector3 nn = Vector3Normalize({-dx * str, -dy * str, 1});
        n[y * S + x] = {(unsigned char)((nn.x * 0.5f + 0.5f) * 255), (unsigned char)((nn.y * 0.5f + 0.5f) * 255), (unsigned char)((nn.z * 0.5f + 0.5f) * 255), 255};
    }
}

static Texture2D texD[4], texN[4];
static void LoadMaterials() {
    const char* names[] = {"asphalt", "paving", "sand", "plaster"};
    for (int i = 0; i < 4; i++) {
        std::string base = std::string(GetApplicationDirectory()) + "assets/" + names[i];
        Image d, n;
        if (FileExists((base + "_diff.jpg").c_str()) && FileExists((base + "_nor.jpg").c_str())) {
            d = LoadImage((base + "_diff.jpg").c_str()); n = LoadImage((base + "_nor.jpg").c_str());
            TraceLog(LOG_INFO, "Material fotografico: %s", names[i]);
        } else GenMaterialImages(i, d, n);
        texD[i] = LoadTextureFromImage(d); texN[i] = LoadTextureFromImage(n);
        UnloadImage(d); UnloadImage(n);
        for (Texture2D* t : {&texD[i], &texN[i]}) { GenTextureMipmaps(t); SetTextureFilter(*t, TEXTURE_FILTER_ANISOTROPIC_16X); SetTextureWrap(*t, TEXTURE_WRAP_REPEAT); }
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
    mCarBody = LoadModelFromMesh(GenCarBody(4.6f, false));
    mCarSport = LoadModelFromMesh(GenCarBody(4.6f, true));
    mFrond = LoadModelFromMesh(GenFrond());
    mRim = LoadModelFromMesh(GenMeshCylinder(0.25f, 0.27f, 20));
    Model* all[] = {&mCube, &mCyl, &mSphere, &mPlane, &mCarBody, &mCarSport, &mFrond, &mRim};
    LoadMaterials();
    LoadExternalModels();
    const char* tnames[] = {"tAsphD", "tPaveD", "tSandD", "tWallD", "tAsphN", "tPaveN", "tSandN", "tWallN"};
    for (int i = 0; i < 8; i++) { int sl = 1 + i; SetShaderValue(lit, GetShaderLocation(lit, tnames[i]), &sl, SHADER_UNIFORM_INT); }
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
            Color neons[] = {{255, 40, 160, 255}, {40, 230, 255, 255}, {255, 140, 30, 255}, {150, 60, 255, 255}};
            buildings.push_back({pos, {w, h, d}, downtown && rand() % 2 ? Color{180, 195, 205, 255} : pastel[rand() % 7], downtown ? 0 : 1 + rand() % 2, neons[rand() % 4]});
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
    if (getenv("NC_X")) { player.x = (float)atof(getenv("NC_X")); playerCar.pos.x = player.x - 4; }
    if (getenv("NC_YAW")) camYaw = (float)atof(getenv("NC_YAW"));
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
        Vector3 lightCol = sunH > -0.05f ? Vector3Scale(sunCol, Clamp(sunH * 8, 0, 1)) : Vector3{0.18f, 0.22f, 0.4f};
        Vector3 zen = Vector3Lerp({0.01f, 0.015f, 0.05f}, {0.12f, 0.32f, 0.75f}, day);
        Vector3 hor = Vector3Lerp(Vector3Lerp({0.04f, 0.04f, 0.09f}, {0.65f, 0.8f, 0.95f}, day), {1.1f, 0.45f, 0.3f}, dusk * 0.8f);
        Vector3 ambS = Vector3Lerp({0.07f, 0.08f, 0.15f}, {0.35f, 0.45f, 0.6f}, day); ambS = Vector3Lerp(ambS, {0.35f, 0.22f, 0.3f}, dusk * 0.5f);
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
                drawM(mCube, TRS({cx, 0.075f, cz}, 0, {BLOCK, 0.15f, BLOCK}), {200, 198, 192, 255}, 0, 0.85f, 3);
                Vector3 lp = {cx + P / 2 - 3.f, 0, cz + P / 2 - 3.f};
                drawM(mCube, TRS({lp.x, 4, lp.z}, 0, {0.18f, 8, 0.18f}), {50, 52, 55, 255}, 0, 0.4f);
                drawM(mCube, TRS({cx + P / 2 + 4.f, 7.4f, cz + P / 2 + 4.f}, 0, {0.6f, 0.15f, 0.6f}), {255, 220, 170, 255}, night * 6, 0.2f);
            }
            for (auto& b : buildings) {
                if (Vector2Distance({b.pos.x, b.pos.z}, {focusPt.x, focusPt.z}) > cullR + 80) continue;
                drawM(mCube, TRS(b.pos, 0, b.size), b.col, 0, 0.75f, 2);
                drawM(mCube, TRS({b.pos.x, b.size.y + 0.4f, b.pos.z}, 0, {b.size.x + 0.4f, 0.8f, b.size.z + 0.4f}), {225, 222, 215, 255}, 0, 0.8f);
                if (b.size.y > 40) drawM(mCube, TRS({b.pos.x, b.size.y + 3.f, b.pos.z}, 0, {b.size.x * 0.4f, 5, b.size.z * 0.4f}), {120, 125, 130, 255}, 0, 0.5f);
                if (b.style > 0) {   // art deco: frisos por andar, toldo, letreiro neon
                    Color trim = {245, 243, 236, 255};
                    for (float y = 3.6f; y < b.size.y - 1; y += 3.6f)
                        drawM(mCube, TRS({b.pos.x, y + 0.05f, b.pos.z}, 0, {b.size.x + 0.35f, 0.22f, b.size.z + 0.35f}), trim, 0, 0.7f);
                    drawM(mCube, TRS({b.pos.x, 3.2f, b.pos.z + b.size.z / 2 + 0.9f}, 0, {b.size.x * 0.8f, 0.12f, 1.8f}), b.neon, 0, 0.6f);
                    float sw = fminf(b.size.x * 0.6f, 9.f);
                    drawM(mCube, TRS({b.pos.x, b.size.y - 1.4f, b.pos.z + b.size.z / 2 + 0.15f}, 0, {sw, 0.35f, 0.15f}), b.neon, 0.4f + night * 9, 0.3f);
                    if (b.style == 2) {
                        drawM(mCube, TRS({b.pos.x, b.size.y / 2 + 1, b.pos.z + b.size.z / 2 + 0.3f}, 0, {1.2f, b.size.y + 2, 0.6f}), trim, 0, 0.7f);
                        drawM(mCube, TRS({b.pos.x, b.size.y / 2 + 1, b.pos.z + b.size.z / 2 + 0.62f}, 0, {0.18f, b.size.y, 0.08f}), b.neon, 0.4f + night * 9, 0.3f);
                    }
                } else {
                    for (int k = 0; k < 3; k++) drawM(mCube, TRS({b.pos.x + (k - 1) * b.size.x * 0.25f, b.size.y + 1.2f, b.pos.z - b.size.z * 0.25f}, 0, {2.2f, 1.6f, 2.2f}), {150, 152, 155, 255}, 0, 0.5f);
                }
            }
            for (auto& p : palms) {
                if (Vector2Distance({p.pos.x, p.pos.z}, {focusPt.x, focusPt.z}) > cullR) continue;
                if (extPalm.ok) { drawExt(extPalm, TRS(p.pos, p.pos.x, {p.h / 10.f, p.h / 10.f, p.h / 10.f}), -1, 0, 0.7f); continue; }
                Matrix lean = MatrixRotateZ(p.lean);
                for (int s = 0; s < 14; s++) {
                    float y0 = p.h * (s + 0.5f) / 14.f, rr = 0.26f - s * 0.006f;
                    Vector3 o = Vector3Transform({0, y0, 0}, lean);
                    drawM(mSphere, MatrixMultiply(MatrixMultiply(MatrixScale(rr, p.h / 22.f, rr), lean), MatrixTranslate(p.pos.x + o.x, p.pos.y + o.y, p.pos.z + o.z)), {130, 108, 82, 255}, 0, 0.95f);
                }
                Vector3 top = Vector3Add(p.pos, Vector3Transform({0, p.h, 0}, lean));
                drawM(mSphere, TRS(top, 0, {0.4f, 0.35f, 0.4f}), {90, 75, 45, 255}, 0, 0.9f);
                for (int i = 0; i < 11; i++) {
                    float a = i * 2 * PI / 11 + p.pos.x, sway = sinf(t * 1.1f + p.pos.z + i * 0.7f) * 0.05f;
                    Matrix fr = MatrixMultiply(MatrixMultiply(MatrixRotateX(-0.25f + sway + (i % 3) * 0.18f), MatrixRotateY(a)), MatrixTranslate(top.x, top.y, top.z));
                    drawM(mFrond, fr, WHITE, 0, 0.6f);
                }
            }
            for (auto& c : cars) if (Vector3Distance(c.pos, focusPt) < cullR) drawCar(c, t, night);
            if (!(screen == TITLE)) drawCar(playerCar, t, night);
            else drawCar(playerCar, t, night);
            for (auto& p : peds) {
                if (Vector3Distance(p.pos, focusPt) > 200) continue;
                if (p.dead) { drawM(mCube, TRS({p.pos.x, 0.3f, p.pos.z}, p.yaw, {0.5f, 0.3f, 1.7f}), p.shirt, 0, 0.8f); continue; }
                drawHuman(p.pos, p.yaw, p.phase, 1, p.shirt, {50, 60, 80, 255}, {200, 150, 115, 255},
                          extPeds.empty() ? nullptr : &extPeds[(&p - &peds[0]) % extPeds.size()], Vector3Distance(p.pos, focusPt) < 30);
            }
            if (!inCar && screen != TITLE) drawHuman(player, pyaw, walkPhase, moving, {240, 240, 235, 255}, {40, 45, 60, 255}, {190, 140, 105, 255}, extPlayer.ok ? &extPlayer : nullptr);
        };

        // =============================== Sombra ===============================
        Matrix lightVP = MatrixIdentity();
        if (shadowsOn && lightDir.y > 0.02f) {
            Camera3D lc = {}; lc.up = {0, 1, 0}; lc.projection = CAMERA_ORTHOGRAPHIC; lc.fovy = screen == TITLE ? 700 : 220;
            Vector3 c = screen == TITLE ? Vector3{0, 0, 0} : Vector3{cam.target.x, 0, cam.target.z};
            lc.target = c; lc.position = Vector3Add(c, Vector3Scale(lightDir, 400));
            for (Model* m : all) m->materials[0].shader = depth;
            SetExtShader(depth);
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
            SetExtShader(lit);
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
        int slot = 10; rlActiveTextureSlot(slot); rlEnableTexture(shadowMap.depth.id);
        for (int i = 0; i < 4; i++) { rlActiveTextureSlot(1 + i); rlEnableTexture(texD[i].id); rlActiveTextureSlot(5 + i); rlEnableTexture(texN[i].id); }
        rlActiveTextureSlot(0);
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
