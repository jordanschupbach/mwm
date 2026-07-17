#!/usr/bin/env bash
# Writes/clears the small JSON status files that mwm's agent sidebar polls.
#
# Claude Code usage (in ~/.claude/settings.json hooks):
#   agent-status-hook.sh claude <HookEventName>
#   The hook JSON payload is read from stdin, per Claude Code's hook contract.
#
# Codex usage (as the `notify` command in ~/.codex/config.toml):
#   agent-status-hook.sh codex
#   Codex appends a JSON payload as the final argv entry.
set -u

status_dir="${XDG_RUNTIME_DIR:-/tmp}/mwm-agents"
mkdir -p "$status_dir" 2>/dev/null

# Pulls "field": "value" out of flat, single-line JSON without needing jq/python.
json_field() {
  grep -oP "\"$1\"\s*:\s*\"\K(\\\\.|[^\"\\\\])*" <<<"$2" | head -n1
}

json_escape() {
  local s=$1
  s=${s//\\/\\\\}
  s=${s//\"/\\\"}
  s=${s//$'\n'/ }
  s=${s//$'\r'/ }
  printf '%s' "$s"
}

sanitize_id() {
  printf '%s' "$1" | tr -c 'A-Za-z0-9_-' '_'
}

write_status() {
  local id=$1 agent=$2 status=$3 label=$4 cwd=$5
  local file="$status_dir/$(sanitize_id "$id").json"
  local tmp="$file.tmp.$$"
  printf '{"agent":"%s","status":"%s","label":"%s","cwd":"%s","updated":%d}\n' \
    "$(json_escape "$agent")" "$(json_escape "$status")" \
    "$(json_escape "$label")" "$(json_escape "$cwd")" "$(date +%s)" >"$tmp" 2>/dev/null
  mv -f "$tmp" "$file" 2>/dev/null
}

agent_kind=${1:-}

case "$agent_kind" in
claude)
  event=${2:-}
  payload=$(cat)
  session_id=$(json_field session_id "$payload")
  cwd=$(json_field cwd "$payload")
  tool_name=$(json_field tool_name "$payload")
  message=$(json_field message "$payload")
  [ -n "$session_id" ] || exit 0
  file_id="claude-$session_id"

  case "$event" in
  SessionStart)
    write_status "$file_id" claude running "starting up" "$cwd"
    ;;
  UserPromptSubmit)
    write_status "$file_id" claude running "processing your message" "$cwd"
    ;;
  PreToolUse | PostToolUse)
    write_status "$file_id" claude running "running ${tool_name:-a tool}" "$cwd"
    ;;
  Notification)
    write_status "$file_id" claude needs_input "${message:-needs your attention}" "$cwd"
    ;;
  Stop | SubagentStop)
    write_status "$file_id" claude needs_input "waiting for you" "$cwd"
    ;;
  SessionEnd)
    rm -f "$status_dir/$(sanitize_id "$file_id").json" 2>/dev/null
    ;;
  esac
  ;;
codex)
  payload=${*: -1}
  turn_type=$(json_field type "$payload")
  thread_id=$(json_field thread-id "$payload")
  cwd=$(json_field cwd "$payload")
  last_message=$(json_field last-assistant-message "$payload")
  [ -n "$thread_id" ] || exit 0
  file_id="codex-$thread_id"

  case "$turn_type" in
  agent-turn-complete)
    label="waiting for you"
    if [ -n "$last_message" ]; then
      label="${last_message:0:80}"
    fi
    write_status "$file_id" codex needs_input "$label" "$cwd"
    ;;
  *)
    write_status "$file_id" codex running "${turn_type:-working}" "$cwd"
    ;;
  esac
  ;;
esac

exit 0
