
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

# Instalar GNOME, servidor gráfico e acesso web
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ubuntu-desktop-minimal \
      gnome-session \
      gnome-shell \
      gnome-terminal \
      nautilus \
      gnome-control-center \
      gnome-settings-daemon \
      gnome-keyring \
      dbus-x11 \
      dbus-user-session \
      xauth \
      xvfb \
      x11vnc \
      novnc \
      websockify \
      firefox \
      sudo \
      curl \
      wget \
      ca-certificates \
      procps \
      psmisc \
      fonts-dejavu-core \
      fonts-noto \
      mesa-utils \
      libgl1-mesa-dri \
      adwaita-icon-theme \
      yaru-theme-gnome-shell \
      yaru-theme-gtk \
      yaru-theme-icon \
      yaru-theme-sound && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Ubuntu 24.04 pode já possuir o usuário ubuntu
RUN id ubuntu >/dev/null 2>&1 || \
    useradd -m -s /bin/bash ubuntu

RUN mkdir -p /home/ubuntu/.vnc \
    /tmp/runtime-ubuntu \
    /home/ubuntu/Desktop && \
    chmod 700 /tmp/runtime-ubuntu && \
    chown -R ubuntu:ubuntu /home/ubuntu \
    /tmp/runtime-ubuntu

# Configuração de sessão GNOME
RUN printf '%s\n' \
    '[Desktop Entry]' \
    'Name=Ubuntu GNOME' \
    'Exec=gnome-session' \
    'Type=Application' \
    > /usr/share/xsessions/ubuntu-cloud.desktop

# Script de inicialização embutido no Dockerfile
RUN cat > /usr/local/bin/start-desktop <<'EOF'
#!/bin/bash
set -Eeuo pipefail

export DISPLAY=:1
export HOME=/home/ubuntu
export XDG_RUNTIME_DIR=/tmp/runtime-ubuntu
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe

PORT="${PORT:-8080}"

if [[ ! "$PORT" =~ ^[0-9]+$ ]]; then
  echo "ERRO: PORT inválida"
  exit 1
fi

if [ -z "${DESKTOP_PASSWORD:-}" ]; then
  echo "ERRO: Configure DESKTOP_PASSWORD na Railway."
  exit 1
fi

if [ "${#DESKTOP_PASSWORD}" -lt 8 ]; then
  echo "ERRO: DESKTOP_PASSWORD deve ter 8 caracteres ou mais."
  exit 1
fi

mkdir -p /tmp/runtime-ubuntu
chown ubuntu:ubuntu /tmp/runtime-ubuntu
chmod 700 /tmp/runtime-ubuntu

mkdir -p /home/ubuntu/.vnc
chown -R ubuntu:ubuntu /home/ubuntu/.vnc

# VNC tradicional utiliza apenas os primeiros
# 8 caracteres da senha para autenticação.
x11vnc -storepasswd \
  "$DESKTOP_PASSWORD" \
  /home/ubuntu/.vnc/passwd >/dev/null

chmod 600 /home/ubuntu/.vnc/passwd
chown ubuntu:ubuntu /home/ubuntu/.vnc/passwd

echo "Iniciando servidor gráfico Xvfb..."
Xvfb :1 \
  -screen 0 1600x900x24 \
  -ac \
  +extension GLX \
  +render \
  -noreset &

sleep 3

echo "Iniciando sessão GNOME..."
su -s /bin/bash ubuntu -c '
  export DISPLAY=:1
  export XDG_RUNTIME_DIR=/tmp/runtime-ubuntu
  export HOME=/home/ubuntu
  export LIBGL_ALWAYS_SOFTWARE=1
  export GALLIUM_DRIVER=llvmpipe
  export XDG_SESSION_TYPE=x11
  export XDG_CURRENT_DESKTOP=GNOME
  export GNOME_SHELL_SESSION_MODE=ubuntu

  dbus-run-session -- bash -c "
    gsettings set org.gnome.desktop.interface \
      color-scheme prefer-dark || true

    gnome-session --session=ubuntu
  "
' &

echo "Iniciando VNC..."
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

echo "Iniciando noVNC na porta $PORT..."

exec websockify \
  --web=/usr/share/novnc \
  "0.0.0.0:$PORT" \
  localhost:5900
EOF

RUN chmod +x /usr/local/bin/start-desktop

EXPOSE 8080

CMD ["/usr/local/bin/start-desktop"]
