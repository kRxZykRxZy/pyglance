#!/bin/sh
set -eu

# Pi Monitor production asset updater.
# Copies the repository's production HTML/CSS/JS into the live web root.

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WEB_ROOT=${PI_MONITOR_WEB_ROOT:-/usr/share/pi-monitor/web}

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root: sudo $0"
    exit 1
fi

mkdir -p "$WEB_ROOT"

install -m 0644 "$SCRIPT_DIR/login.html" "$WEB_ROOT/login.html"
install -m 0644 "$SCRIPT_DIR/dashboard.html" "$WEB_ROOT/dashboard.html"
install -m 0644 "$SCRIPT_DIR/app.css" "$WEB_ROOT/app.css"
install -m 0644 "$SCRIPT_DIR/app.js" "$WEB_ROOT/app.js"

# Ensure the browser gets the newest asset instead of an old cached copy.
# The JS itself uses cache-busting API requests; these query strings bust
# browser caches for the static CSS/JS files.
TMP="$WEB_ROOT/dashboard.html.tmp"
sed 's#href="/app.css"#href="/app.css?v=__ASSET_VERSION__"#g; s#src=/app.js#src=/app.js?v=__ASSET_VERSION__#g; s#src="/app.js"#src="/app.js?v=__ASSET_VERSION__"#g' "$SCRIPT_DIR/dashboard.html" > "$TMP"
install -m 0644 "$TMP" "$WEB_ROOT/dashboard.html"
rm -f "$TMP"

TMP="$WEB_ROOT/login.html.tmp"
sed 's#href="/app.css"#href="/app.css?v=__ASSET_VERSION__"#g; s#src=/app.js#src=/app.js?v=__ASSET_VERSION__#g; s#src="/app.js"#src="/app.js?v=__ASSET_VERSION__"#g' "$SCRIPT_DIR/login.html" > "$TMP"
install -m 0644 "$TMP" "$WEB_ROOT/login.html"
rm -f "$TMP"

# Replace the placeholder with the current Unix timestamp.
VERSION=$(date +%s)
for f in "$WEB_ROOT/login.html" "$WEB_ROOT/dashboard.html"; do
    sed -i "s/__ASSET_VERSION__/$VERSION/g" "$f"
done

# Reload the service if it is installed/running. Static assets do not require
# a binary rebuild, but restarting also clears any server-side state.
if command -v systemctl >/dev/null 2>&1 && systemctl cat pi-monitor.service >/dev/null 2>&1; then
    systemctl restart pi-monitor.service
fi

echo "Pi Monitor production assets updated. Version: $VERSION"
echo "Live root: $WEB_ROOT"
