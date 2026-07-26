#!/usr/bin/env bash
# Provenance rules of docs/standards/ai/provenance.md, applied three ways:
#
#   check-provenance.sh [RANGE]        commit range (default origin/main..HEAD),
#                                      the verify.sh / CI stage
#   check-provenance.sh --message F    one commit-message file, used by the
#                                      scripts/hooks/commit-msg hook
#   check-provenance.sh --selftest     prove the rules fail on known-bad input
#                                      before anything trusts them
#
# Rule 1: an assistance trailer requires a human Signed-off-by.
# Rule 2: a Signed-off-by must not name an assistant.
# Co-authored-by counts as an assistance trailer only when it names an
# agent; human co-authors are not assistance.

set -uo pipefail
AGENT_RE='(claude|fable|mythos|opus|sonnet|haiku|copilot|codex|cursor|gemini|chatgpt|gpt-[0-9]|devin|aider)'

# check_msg <message> -> prints violations, returns 1 if any
check_msg() {
    local msg="$1" bad=0

    local assisted=0
    if grep -qiE '^(Assisted-by|Generated-by):' <<<"${msg}"; then
        assisted=1
    elif grep -iE '^Co-authored-by:' <<<"${msg}" | grep -qiE "${AGENT_RE}"; then
        assisted=1
    fi

    if [ "${assisted}" = 1 ] && ! grep -qE '^Signed-off-by:' <<<"${msg}"; then
        echo "assistance trailer without a human Signed-off-by"
        bad=1
    fi
    if grep -iE '^Signed-off-by:' <<<"${msg}" | grep -qiE "${AGENT_RE}"; then
        echo "Signed-off-by names an assistant; only a person can sign off"
        bad=1
    fi
    return "${bad}"
}

selftest() {
    local ok=1

    # known-bad: assisted, no sign-off -> must be rejected
    if check_msg "$(printf 'feat: x\n\nAssisted-by: claude-code:claude-fable-5\n')" >/dev/null; then
        echo "SELFTEST FAIL: assisted commit without sign-off was accepted"
        ok=0
    fi
    # known-bad: an agent signing off -> must be rejected
    if check_msg "$(printf 'feat: x\n\nSigned-off-by: Claude <noreply@anthropic.com>\n')" >/dev/null; then
        echo "SELFTEST FAIL: agent Signed-off-by was accepted"
        ok=0
    fi
    # known-good: assisted + human sign-off -> must pass
    if ! check_msg "$(printf 'feat: x\n\nAssisted-by: claude-code:claude-fable-5\nSigned-off-by: Max Atslega <max@atslega.dev>\n')" >/dev/null; then
        echo "SELFTEST FAIL: valid assisted commit was rejected"
        ok=0
    fi
    # known-good: human co-author, no sign-off -> not assistance, must pass
    if ! check_msg "$(printf 'feat: x\n\nCo-authored-by: Jane Doe <jane@example.org>\n')" >/dev/null; then
        echo "SELFTEST FAIL: human co-author was treated as assistance"
        ok=0
    fi

    if [ "${ok}" = 1 ]; then
        echo "provenance selftest: both rules reject their known-bad inputs"
        return 0
    fi
    return 1
}

case "${1:-}" in
--selftest)
    selftest
    exit "$?"
    ;;
--message)
    msg="$(cat "$2")"
    if out="$(check_msg "${msg}")"; then
        exit 0
    fi
    echo "commit rejected:"
    sed 's/^/  /' <<<"${out}"
    echo "see docs/standards/ai/provenance.md"
    exit 1
    ;;
esac

RANGE="${1:-origin/main..HEAD}"
mapfile -t commits < <(git rev-list "${RANGE}" 2>/dev/null || true)
if [ "${#commits[@]}" -eq 0 ]; then
    echo "provenance: no commits in ${RANGE}"
    exit 0
fi

fail=0
assisted=0
for c in "${commits[@]}"; do
    msg="$(git log -1 --format=%B "${c}")"
    short="$(git log -1 --format='%h %s' "${c}")"
    if grep -qiE '^(Assisted-by|Generated-by):' <<<"${msg}" ||
        grep -iE '^Co-authored-by:' <<<"${msg}" | grep -qiE "${AGENT_RE}"; then
        assisted=$((assisted + 1))
    fi
    if ! out="$(check_msg "${msg}")"; then
        echo "  FAIL ${short}"
        sed 's/^/       /' <<<"${out}"
        fail=1
    fi
done

echo "provenance: ${#commits[@]} commits, ${assisted} disclosed as assisted"
[ "${fail}" = 0 ] || echo "see docs/standards/ai/provenance.md"
exit "${fail}"
