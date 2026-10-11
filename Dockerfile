
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=America/Sao_Paulo \
    DISPLAY=:1 \
    PORT=8080 \
    HOME=/home/ubuntu \
    XDG_RUNTIME_DIR=/tmp/runtime-ubuntu \
    LIBGL_ALWAYS_SOFTWARE=1 \
    GALLIUM_DRIVER=llvmpipe \
    NO_AT_BRIDGE=1

# Sistema, interface GNOME e acesso remoto
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ubuntu-desktop-minimal \
    gnome-session \
    gnome-shell \
    gnome-terminal \
    nautilus \
    gnome-control-center \
    gnome-settings-daemon \
    dbus-x11 \
    dbus-user-session \
    xauth \
    xvfb \
    x11vnc \
    novnc \
    websockify \
    sudo \
    curl \
    wget \
    ca-certificates \
    procps \
    psmisc \
    fonts-noto \
    mesa-utils \
    libgl1-mesa-dri \
    yaru-theme-gtk \
    yaru-theme-icon \
    yaru-theme-gnome-shell \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Usuário ubuntu já pode existir na imagem
RUN id ubuntu >/dev/null 2>&1 || \
    useradd -m -s /bin/bash ubuntu

RUN mkdir -p \
    /home/ubuntu/.vnc \
    /home/ubuntu/Desktop \
    /tmp/runtime-ubuntu && \
    chown -R ubuntu:ubuntu \
    /home/ubuntu \
    /tmp/runtime-ubuntu && \
    chmod 700 /tmp/runtime-ubuntu

# Inicialização em um único Dockerfile
RUN cat > /usr/local/bin/start-desktop <<'EOF'
#!/bin/bash
set -Eeuo pipefail

export DISPLAY=:1
export HOME=/home/ubuntu
export XDG_RUNTIME_DIR=/tmp/runtime-ubuntu
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe

# Porta fixa para evitar erro de variável
PORT=8080

echo "Iniciando Ubuntu GNOME..."
echo "Porta pública: $PORT"

if [ -z "${DESKTOP_PASSWORD:-}" ]; then
    echo "ERRO: Configure DESKTOP_PASSWORD na Railway."
    exit 1
fi

mkdir -p /tmp/runtime-ubuntu
chown ubuntu:ubuntu /tmp/runtime-ubuntu
chmod 700 /tmp/runtime-ubuntu

mkdir -p /home/ubuntu/.vnc
chown -R ubuntu:ubuntu /home/ubuntu/.vnc

# Criar senha VNC
x11vnc -storepasswd \
    "$DESKTOP_PASSWORD" \
    /home/ubuntu/.vnc/passwd >/dev/null

chmod 600 /home/ubuntu/.vnc/passwd
chown ubuntu:ubuntu /home/ubuntu/.vnc/passwd

# Iniciar servidor gráfico
echo "Iniciando Xvfb..."

Xvfb :1 \
    -screen 0 1600x900x24 \
    -ac \
    +extension GLX \
    +render \
    -noreset &

sleep 3

# Iniciar sessão GNOME
echo "Iniciando GNOME..."

su -s /bin/bash ubuntu -c '
    export DISPLAY=:1
    export HOME=/home/ubuntu
    export XDG_RUNTIME_DIR=/tmp/runtime-ubuntu
    export XDG_SESSION_TYPE=x11
    export XDG_CURRENT_DESKTOP=GNOME
    export GNOME_SHELL_SESSION_MODE=ubuntu
    export LIBGL_ALWAYS_SOFTWARE=1
    export GALLIUM_DRIVER=llvmpipe

    dbus-run-session -- \
      gnome-session --session=ubuntu
' &

sleep 5

# Servidor VNC interno
echo "Iniciando x11vnc..."

x11vnc \
    -display :1 \
    -rfbauth /home/ubuntu/.vnc/passwd \
    -rfbport 5900 \
    -localhost \
    -forever \
    -shared \
    -noxdamage \
    -xkb &

sleep 2

# Acesso pelo navegador
echo "Iniciando noVNC na porta 8080..."

exec websockify \
    --web=/usr/share/novnc \
    0.0.0.0:8080 \
    localhost:5900
EOF

RUN chmod +x /usr/local/bin/start-desktop

EXPOSE 8080

CMD ["/usr/local/bin/start-desktop"]
