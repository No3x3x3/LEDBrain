#!/usr/bin/env bash
set -e
git add -A
git commit -m "$(cat <<'EOF'
Expose LEDBrain UI entry in C6 setup

Add an AP-side panel and redirect link so users can reach the P4 UI from the ESP32-C6 provisioning page. Also normalize the setup page HTML output by removing the BOM from app.js.

EOF
)"
git status
git push origin main
