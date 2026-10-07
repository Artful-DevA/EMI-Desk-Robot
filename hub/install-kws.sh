#!/usr/bin/env bash
set -euo pipefail

ROOT="$HOME/.local/share"
VENV="$ROOT/emi-hub-venv"
KWS_BASE="$ROOT/emi-kws"
MODEL_NAME="sherpa-onnx-kws-zipformer-gigaspeech-3.3M-2024-01-01"
MODEL_DIR="$KWS_BASE/$MODEL_NAME"
ARCHIVE="$KWS_BASE/$MODEL_NAME.tar.bz2"

MODEL_URL="https://github.com/k2-fsa/sherpa-onnx/releases/download/kws-models/$MODEL_NAME.tar.bz2"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REQUIREMENTS="$SCRIPT_DIR/requirements-kws.txt"

mkdir -p "$ROOT"
mkdir -p "$KWS_BASE"

if ! python3 -m venv "$VENV" >/dev/null 2>&1; then
  echo
  echo "ERROR: Python venv support is missing."
  echo "On Debian/Raspberry Pi OS install it with:"
  echo "  sudo apt install python3-venv"
  exit 1
fi

"$VENV/bin/python" -m pip install --upgrade pip
"$VENV/bin/python" -m pip install -r "$REQUIREMENTS"

if [[ ! -d "$MODEL_DIR" ]]; then
  echo
  echo "Downloading the small English EMI keyword-spotting model..."

  curl -fL     "$MODEL_URL"     -o "$ARCHIVE"

  tar -xjf     "$ARCHIVE"     -C "$KWS_BASE"

  rm -f "$ARCHIVE"
fi

for required in   "$MODEL_DIR/encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx"   "$MODEL_DIR/decoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx"   "$MODEL_DIR/joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx"   "$MODEL_DIR/tokens.txt"   "$MODEL_DIR/bpe.model"
do
  if [[ ! -f "$required" ]]; then
    echo
    echo "ERROR: keyword model file missing:"
    echo "  $required"
    exit 1
  fi
done

RAW_KEYWORDS="$MODEL_DIR/keywords_emi_raw.txt"
KEYWORDS="$MODEL_DIR/keywords_emi.txt"

cat > "$RAW_KEYWORDS" <<'EOF'
EMI :1.8 #0.25 @EMI
EMMY :1.8 #0.25 @EMI
EMMIE :1.8 #0.25 @EMI
EOF

"$VENV/bin/sherpa-onnx-cli" text2token   --tokens "$MODEL_DIR/tokens.txt"   --tokens-type bpe   --bpe-model "$MODEL_DIR/bpe.model"   "$RAW_KEYWORDS"   "$KEYWORDS"

if [[ ! -s "$KEYWORDS" ]]; then
  echo
  echo "ERROR: EMI keyword file was not generated."
  exit 1
fi

echo
echo "EMI keyword spotter is ready."
echo "Model:"
echo "  $MODEL_DIR"
echo "Keywords:"
cat "$KEYWORDS"
