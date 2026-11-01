#!/usr/bin/env bash
# tools/install-icons.py against a model reader: adds only our icons, view.json
# entries and names, keeps everything else, backs up once, is idempotent, and
# refuses a view.json it cannot parse.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
mkdir -p "$t/applications" "$t/system/config/desktop" "$t/system/language"
# Shape of the firmware file per the MobileRead wiki: an app table and groups.
cat >"$t/system/config/desktop/view.json" <<'EOF'
{
    "applications": {
        "PB_Library": {"path": "/ebrmain/bin/library.app", "title": "@Library"},
        "PB_Settings": {"path": "/ebrmain/bin/settings.app", "title": "@Settings"},
        "U_inkquest": {"path": "/mnt/ext1/applications/inkquest.app", "title": "InkQuest"}
    },
    "_comment": "groups",
    "groups": [
        {"title": "@General", "sort": "title", "apps": ["PB_Library", "PB_Settings"]},
        {"title": "@User", "sort": "title", "apps": ["U_inkquest"]}
    ]
}
EOF
printf 'language=nl\n' >"$t/system/config/global.cfg"
printf '@inkquest=InkQuest\n' >"$t/system/language/nl.txt"
orig=$(cat "$t/system/config/desktop/view.json")

fail=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }

python3 -I "$root/tools/install-icons.py" "$t" >/dev/null
v=$t/system/config/desktop/view.json
check "icons copied" '[[ -f $t/applications/icons/mmiv.bmp && -f $t/applications/icons/mmiv-keypad_f.bmp ]]'
check "backup holds the original" '[[ "$(cat "$v.mmiv-backup")" == "$orig" ]]'
check "our entries added, others kept" 'python3 -I -c "
import json,sys; v=json.load(open(sys.argv[1])); a=v[\"applications\"]
assert a[\"U_mmiv\"][\"path\"]==\"/mnt/ext1/applications/mmiv.app\"
assert a[\"U_mmiv\"][\"focused_icon\"].endswith(\"/icons/mmiv_f.bmp\")
assert \"U_mmiv-keypad\" in a and \"PB_Library\" in a and \"U_inkquest\" in a
g=v[\"groups\"]; assert g[0][\"apps\"]==[\"PB_Library\",\"PB_Settings\"]
assert g[1][\"apps\"]==[\"U_inkquest\",\"U_mmiv\",\"U_mmiv-keypad\"]
" "$v"'
check "names in the device language (nl), existing line kept" \
	'grep -qx "@mmiv=Might and Magic IV" "$t/system/language/nl.txt" && grep -qx "@inkquest=InkQuest" "$t/system/language/nl.txt"'
before=$(cat "$v")
python3 -I "$root/tools/install-icons.py" "$t" >/dev/null
check "second run changes nothing" '[[ "$(cat "$v")" == "$before" && $(grep -c "^@mmiv=" "$t/system/language/nl.txt") == 1 ]]'
check "backup not overwritten" '[[ "$(cat "$v.mmiv-backup")" == "$orig" ]]'
printf '{ broken' >"$v"
check "invalid view.json is refused and left alone" \
	'! python3 -I "$root/tools/install-icons.py" "$t" 2>/dev/null && [[ "$(cat "$v")" == "{ broken" ]]'
exit $fail
