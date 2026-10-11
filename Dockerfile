FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive \
    TZ=America/Sao_Paulo \
    PORT=8080 \
    DISPLAY=:1 \
    XDG_RUNTIME_DIR=/tmp/runtime-ubuntu \
    LIBGL_ALWAYS_SOFTWARE=1 \
    GSK_RENDERER=cairo

RUN apt-get update && apt-get install -y --no-install-recommends \
    ubuntu-desktop-minimal gnome-session gnome-shell gnome-terminal \
    dbus-x11 dbus-user-session xserver-xorg-core xserver-xorg-video-dummy \
    xvfb x11vnc novnc websockify python3 supervisor \
    sudo ca-certificates fonts-dejavu-core adwaita-icon-theme \
    && apt-get clean && rm -rf /var/lib/apt/lists/* \
    && (id ubuntu >/dev/null 2>&1 || useradd -m -s /bin/bash ubuntu) \
    && mkdir -p /data /tmp/runtime-ubuntu /home/ubuntu/.config \
    && chmod 700 /tmp/runtime-ubuntu \
    && chown -R ubuntu:ubuntu /home/ubuntu /tmp/runtime-ubuntu \
    && ln -sf /usr/share/novnc/vnc.html /usr/share/novnc/index.html

RUN cat > /usr/local/bin/start-desktop <<'SCRIPT'
#!/bin/bash
set -euo pipefail
: "${DESKTOP_PASSWORD:?Set DESKTOP_PASSWORD in Railway variables}"
mkdir -p /data /home/ubuntu/.config /tmp/runtime-ubuntu
chown ubuntu:ubuntu /tmp/runtime-ubuntu
chmod 700 /tmp/runtime-ubuntu
if [ ! -e /data/home ]; then
  mkdir -p /data/home
  cp -a /home/ubuntu/. /data/home/
fi
chown -R ubuntu:ubuntu /data/home
if [ ! -L /home/ubuntu ]; then
  rm -rf /home/ubuntu
  ln -s /data/home /home/ubuntu
fi
mkdir -p /run/dbus
if [ ! -S /run/dbus/system_bus_socket ]; then
  dbus-daemon --system --fork || true
fi
x11vnc -storepasswd "$DESKTOP_PASSWORD" /tmp/vnc.pass >/dev/null
chmod 600 /tmp/vnc.pass
exec /usr/bin/supervisord -n -c /etc/supervisor/supervisord.conf
SCRIPT
RUN chmod +x /usr/local/bin/start-desktop

RUN cat > /usr/local/bin/start-gnome <<'SCRIPT'
#!/bin/bash
export HOME=/home/ubuntu
export USER=ubuntu
export XDG_SESSION_TYPE=x11
export XDG_CURRENT_DESKTOP=GNOME
export GNOME_SHELL_SESSION_MODE=ubuntu
export XDG_RUNTIME_DIR=/tmp/runtime-ubuntu
export DISPLAY=:1
export LIBGL_ALWAYS_SOFTWARE=1
export MUTTER_DEBUG_FORCE_KMS_MODE=simple
sleep 4
exec dbus-run-session -- gnome-session --session=ubuntu
SCRIPT
RUN chmod +x /usr/local/bin/start-gnome

RUN cat > /etc/supervisor/supervisord.conf <<'EOF2'
[supervisord]
nodaemon=true
logfile=/dev/null
logfile_maxbytes=0

[program:xvfb]
command=/usr/bin/Xvfb :1 -screen 0 1600x900x24 -ac +extension GLX +render -noreset
autostart=true
autorestart=true
priority=10
stdout_logfile=/dev/fd/1
stdout_logfile_maxbytes=0
stderr_logfile=/dev/fd/2
stderr_logfile_maxbytes=0

[program:gnome]
command=/usr/bin/su -s /bin/bash ubuntu -c /usr/local/bin/start-gnome
autostart=true
autorestart=true
priority=20
stdout_logfile=/dev/fd/1
stdout_logfile_maxbytes=0
stderr_logfile=/dev/fd/2
stderr_logfile_maxbytes=0

[program:vnc]
command=/usr/bin/x11vnc -display :1 -rfbauth /tmp/vnc.pass -rfbport 5900 -localhost -forever -shared -noxdamage
autostart=true
autorestart=true
priority=30
stdout_logfile=/dev/fd/1
stdout_logfile_maxbytes=0
stderr_logfile=/dev/fd/2
stderr_logfile_maxbytes=0

[program:web]
command=/usr/bin/websockify --web=/usr/share/novnc/ 0.0.0.0:%(ENV_PORT)s localhost:5900
autostart=true
autorestart=true
priority=40
stdout_logfile=/dev/fd/1
stdout_logfile_maxbytes=0
stderr_logfile=/dev/fd/2
stderr_logfile_maxbytes=0
EOF2

EXPOSE 8080
VOLUME ["/data"]
CMD ["/usr/local/bin/start-desktop"]
