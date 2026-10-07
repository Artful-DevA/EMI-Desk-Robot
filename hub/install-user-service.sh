#!/usr/bin/env bash
set -euo pipefail

SERVICE_DIR="$HOME/.config/systemd/user"
CONFIG_DIR="$HOME/.config/emi-hub"
ENV_FILE="$CONFIG_DIR/emi-hub.env"

mkdir -p "$SERVICE_DIR"
mkdir -p "$CONFIG_DIR"

if [ ! -f "$ENV_FILE" ]; then
  TOKEN="$(python3 -c 'import secrets; print(secrets.token_hex(32))')"

  cat > "$ENV_FILE" <<EOF
EMI_HUB_HOST=0.0.0.0
EMI_HUB_PORT=17840
EMI_SHARED_TOKEN=$TOKEN
EOF

  chmod 600 "$ENV_FILE"
fi

cp "$(dirname "$0")/emi-hub.service" "$SERVICE_DIR/emi-hub.service"

systemctl --user daemon-reload
systemctl --user enable emi-hub.service
systemctl --user restart emi-hub.service

echo
echo "EMI Hub installed/updated."
echo "Local health check:"
echo "  curl http://127.0.0.1:17840/health"
echo
echo "Private C3 token is stored in:"
echo "  $ENV_FILE"
echo
echo "Do not paste that token into GitHub or chat."
