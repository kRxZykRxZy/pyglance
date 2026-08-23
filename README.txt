# Pi Monitor

Tiny native ARMv7/armhf web monitor for Raspberry Pi OS 32-bit.

## Build on the Pi
sudo apt update
sudo apt install build-essential
gcc -O2 -s -o pi-monitor pi_monitor.c
sudo install -m 755 pi-monitor /usr/local/bin/pi-monitor
sudo install -m 644 pi-monitor.service /etc/systemd/system/pi-monitor.service
sudo systemctl daemon-reload
sudo systemctl enable --now pi-monitor

Open from another device:
http://PI_IP/

Login:
username: pi
password: Hm361485%

API:
GET /api
POST /api/kill?pid=PID
