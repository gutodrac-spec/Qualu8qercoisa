# Modelos 3D hiper-realistas

Coloque aqui arquivos **.glb** (ou .gltf) e eles substituem automaticamente os modelos gerados por código.
O GitHub Actions empacota esta pasta junto com o `NeonCoast.exe`.

| Nome do arquivo | O que é | Ajuste automático |
|---|---|---|
| `car1.glb` … `car8.glb` | carros do trânsito (e o seu) | comprimento → 4,6 m |
| `police.glb` | viatura de polícia | comprimento → 4,9 m |
| `player.glb` | personagem jogável (com animações) | altura → 1,80 m |
| `pedestrian1.glb` … `pedestrian8.glb` | pedestres (com animações) | altura → 1,78 m |
| `palm.glb` | palmeira | altura → 10 m |

- **Animações:** se o .glb tiver animações cujo nome contém `walk`, `idle` ou `run`, elas são usadas.
  Personagens do **Mixamo** (mixamo.com, grátis) exportados como FBX e convertidos para .glb no Blender funcionam bem.
- **Frente do carro virada errada?** Crie um arquivo de texto `car1.yaw` contendo o ângulo em graus (ex.: `180` ou `90`).
- **Tamanho:** prefira modelos com até ~100 mil triângulos e texturas de 2K (o .glb deve ficar abaixo de ~50 MB; o GitHub recusa arquivos acima de 100 MB).

## Onde achar (verifique sempre a licença: CC0 ou CC-BY, e dê crédito quando for CC-BY)
- **Sketchfab** (sketchfab.com): busque "car", "sedan", "police car", "realistic human", "palm tree"; marque o filtro *Downloadable* e baixe em formato **glTF/GLB**.
- **Poly Haven** (polyhaven.com/models): CC0, ótimos para vegetação e objetos de rua.
- **Fab** (fab.com) e **CGTrader** (filtro "free"): exporte/converta para .glb.

Não use modelos extraídos de jogos comerciais (GTA, etc.): são protegidos por direitos autorais.
