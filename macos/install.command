#!/bin/zsh
# Run explicitly on the Mac that is signed into Claude Code and Codex.
set -eu
SOURCE="${0:A:h}"
PYTHON="${PYTHON:-$(command -v python3)}"
DEVICE="${1:-}"
if [[ -z "$DEVICE" ]]; then
  read 'DEVICE?SmallTV IP address (for example 192.168.1.50): '
fi
[[ "$DEVICE" == http://* ]] || DEVICE="http://$DEVICE"
"$PYTHON" - "$DEVICE" <<'PY'
import sys, urllib.parse
u=urllib.parse.urlparse(sys.argv[1])
if u.scheme!='http' or not u.hostname or u.username or u.password or u.path not in ('','/') or u.query or u.fragment:
 raise SystemExit('Enter the local SmallTV address using http:// and no path.')
if sys.version_info < (3,10): raise SystemExit('Python 3.10 or newer is required.')
PY
INSTALL="$HOME/Library/Application Support/SmallTVUsage"
AGENTS="$HOME/Library/LaunchAgents"
SESSION="gui/$(id -u)"
mkdir -p "$INSTALL/readers" "$AGENTS"
chmod 700 "$INSTALL"
for provider in claude codex; do
  label="com.bzadoienko.smalltv.$provider"
  if launchctl print "$SESSION/$label" >/dev/null 2>&1; then
    launchctl bootout "$SESSION/$label"
  fi
done
cp "$SOURCE/usage_service.py" "$SOURCE/dashboard.html" "$INSTALL/"
cp "$SOURCE/readers/claude.py" "$SOURCE/readers/codex.py" "$SOURCE/readers/LICENSE" "$INSTALL/readers/"
"$PYTHON" -m venv "$INSTALL/.venv"
"$INSTALL/.venv/bin/python" -m pip install -r "$SOURCE/requirements.txt"
"$INSTALL/.venv/bin/python" - "$INSTALL" "$AGENTS" "$DEVICE" <<'PY'
import os, plistlib, sys
from pathlib import Path
runtime, agents, device=map(str,sys.argv[1:])
for provider in ('claude','codex'):
 label='com.bzadoienko.smalltv.'+provider
 value={'Label':label,'ProgramArguments':[runtime+'/.venv/bin/python',runtime+'/usage_service.py','--provider',provider,'--device',device.rstrip('/')],
  'WorkingDirectory':runtime,'RunAtLoad':True,'KeepAlive':True,'ThrottleInterval':15,
  'EnvironmentVariables':{'PATH':'/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin'},
  'StandardOutPath':runtime+'/'+provider+'-launch.log','StandardErrorPath':runtime+'/'+provider+'-launch-error.log'}
 target=Path(agents)/(label+'.plist')
 target.write_bytes(plistlib.dumps(value));os.chmod(target,0o600)
PY
for provider in claude codex; do
  launchctl bootstrap "$SESSION" "$AGENTS/com.bzadoienko.smalltv.$provider.plist"
done
printf '%s\n' 'Enabled both services at Mac login.' 'Dashboard: http://127.0.0.1:8787/'
