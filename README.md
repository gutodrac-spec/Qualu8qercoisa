# Qualu8qercoisa
Qualquer coisa

## Neon Coast (protótipo de jogo 3D em C++/raylib)
Código em `game/`. O `.exe` de Windows é compilado pelo GitHub Actions
(aba **Actions → Build Windows EXE → Artifacts → NeonCoast-Windows**).

Compilar localmente no Windows (Visual Studio + CMake):
```
cmake -S game -B build -A x64
cmake --build build --config Release
```
Controles: WASD mover/dirigir, mouse câmera, Shift correr, E entrar/sair do carro, Espaço freio, ESC pausa.
