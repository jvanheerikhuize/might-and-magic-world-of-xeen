#!/usr/bin/env bash
# tools/install-icons.py against a model reader: adds only our icons, view.json
# entries and names, keeps everything else, backs up once, is idempotent, and
# refuses a view.json it cannot parse or does not recognise.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
mkdir -p "$t/applications" "$t/system/config/desktop" "$t/system/language"
# Shape of the PB628's file (firmware U628.6.9): version, an applications
# table without paths for PB_ apps, and view -> groups.
cat >"$t/system/config/desktop/view.json" <<'EOF'
{
    "version": "6",
    "applications": {
        "_comment": ["Fields - application key id, must start from \"U_\" for user application\n"],
        "PB_Bookstore": {"visible": "0"},
        "PB_Calendar": {"visible": "0"},
        "U_mmiv-keypad": {"path": "applications/mmiv-keypad.app", "title": "old"}
    },
    "view": {
        "_comment": ["\"groups\" - array of application groups\n"],
        "groups": [
            {"title": "@General", "sort": "title", "apps": ["PB_Library", "PB_Settings"]},
            {"title": "@Games", "sort": "title", "apps": ["PB_Chess", "PB_Sudoku", "U_mmiv-keypad"]}
        ]
    }
}
EOF
orig=$(cat "$t/system/config/desktop/view.json")
# An earlier install's keypad launcher, now obsolete.
mkdir -p "$t/applications/icons"
touch "$t/applications/mmiv-keypad.app" "$t/applications/icons/mmiv-keypad.bmp" "$t/applications/icons/mmiv-keypad_f.bmp"
touch "$t/applications/other.app"

fail=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }

python3 -I "$root/tools/install-icons.py" "$t" >/dev/null
v=$t/system/config/desktop/view.json
check "icons copied" '[[ -f $t/applications/icons/mmiv.bmp && -f $t/applications/icons/mmiv_f.bmp ]]'
check "obsolete keypad launcher removed, other apps kept" \
	'[[ ! -e $t/applications/mmiv-keypad.app && ! -e $t/applications/icons/mmiv-keypad.bmp && -f $t/applications/other.app ]]'
check "backup holds the original" '[[ "$(cat "$v.mmiv-backup")" == "$orig" ]]'
check "our entry added (absolute path), keypad entry gone, others kept" 'python3 -I -c "
import json,sys; v=json.load(open(sys.argv[1])); a=v[\"applications\"]
assert a[\"U_mmiv\"][\"path\"]==\"/mnt/ext1/applications/mmiv.app\"
assert a[\"U_mmiv\"][\"icon\"]=={\"path\": \"applications/icons/mmiv.bmp\"}
assert a[\"U_mmiv\"][\"focused_icon\"]=={\"path\": \"applications/icons/mmiv_f.bmp\"}
assert \"U_mmiv-keypad\" not in a and a[\"PB_Calendar\"]=={\"visible\": \"0\"} and \"_comment\" in a
g=v[\"view\"][\"groups\"]; assert g[0][\"apps\"]==[\"PB_Library\",\"PB_Settings\"]
assert g[1][\"apps\"]==[\"PB_Chess\",\"PB_Sudoku\",\"U_mmiv\"]
assert v[\"version\"]==\"6\"
" "$v"'
before=$(cat "$v")
python3 -I "$root/tools/install-icons.py" "$t" >/dev/null
check "second run changes nothing" '[[ "$(cat "$v")" == "$before" ]]'
check "backup not overwritten" '[[ "$(cat "$v.mmiv-backup")" == "$orig" ]]'
printf '{ broken' >"$v"
check "invalid view.json is refused and left alone" \
	'! python3 -I "$root/tools/install-icons.py" "$t" 2>/dev/null && [[ "$(cat "$v")" == "{ broken" ]]'
exit $fail
