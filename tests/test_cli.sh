#!/bin/sh
set -eu

solution=$(./rubik "F R U2 B' L' D' R2 U F' D2")
words=$(printf '%s\n' "$solution" | wc -w)
test "$words" -gt 0
test "$words" -le 30
test -z "$(./rubik "")"

timed_solution=$(./rubik --timed "F R U2 B' L' D'")
test "$(printf '%s\n' "$timed_solution" | wc -w)" -gt 0
two_solution=$(./rubik --2x2 "R U R' U R U2 R' F2")
test "$(printf '%s\n' "$two_solution" | wc -w)" -gt 0
if ./rubik --timed --2x2 "F" >/dev/null 2>&1; then
	exit 1
fi

if ./rubik "F M" >/dev/null 2>&1; then
	exit 1
fi

if ./rubik >/dev/null 2>&1; then
	exit 1
fi

stats=$(./rubik --stats "F R U2 B' L' D'")
printf '%s\n' "$stats" | awk 'NR == 1 {
	if ($1 != "Solution:") exit 1
	count = NF - 1
	next
}
NR == 2 {
	if ($1 != "Mouvements:" || $2 !~ /^[0-9]+$/ || $2 != count || NF != 2) exit 1
	next
}
NR == 3 {
	if ($1 != "Temps:" || $2 !~ /^[0-9]+\.[0-9]+$/ || $3 != "s" || NF != 3) exit 1
	next
}
{ exit 1 }
END { if (NR != 3) exit 1 }'

empty_stats=$(./rubik --stats "")
printf '%s\n' "$empty_stats" | grep -qx 'Mouvements: 0'
if ./rubik --stats "F M" >/dev/null 2>&1; then
	exit 1
fi
if ./rubik --stats >/dev/null 2>&1; then
	exit 1
fi

generated=$(./rubik --generate --length 12 --count 3)
test "$(printf '%s\n' "$generated" | wc -l)" -eq 3
printf '%s\n' "$generated" | awk "NF != 12 { exit 1 } {
	for (i = 1; i <= NF; i++) {
		if (\$i !~ /^[URFDLB](2|')?$/) exit 1
		if (i > 1 && substr(\$i, 1, 1) == substr(\$(i - 1), 1, 1)) exit 1
	}
}"

generated=$(./rubik --generate --count 2)
test "$(printf '%s\n' "$generated" | wc -l)" -eq 2
printf '%s\n' "$generated" | awk 'NF != 25 { exit 1 }'

generated=$(./rubik --generate --length 7)
test "$(printf '%s\n' "$generated" | wc -w)" -eq 7

if ./rubik --generate --length 0 >/dev/null 2>&1; then
	exit 1
fi

generated=$(./rubik --generate --solve --length 8 --count 5)
test "$(printf '%s\n' "$generated" | wc -l)" -eq 14
printf '%s\n' "$generated" | awk "NR % 3 == 1 {
	if (\$1 != \"Mix\" || \$2 != sprintf(\"%d:\", int((NR + 2) / 3)) || NF != 10) exit 1
	for (i = 3; i <= NF; i++)
		if (\$i !~ /^[URFDLB](2|')?$/) exit 1
	next
}
NR % 3 == 2 {
	if (\$1 != \"Solution\" || \$2 != sprintf(\"%d:\", int((NR + 1) / 3)) || NF < 2 || NF > 32) exit 1
	for (i = 3; i <= NF; i++) {
		if (\$i !~ /^[URFDLB](2|')?$/) exit 1
		if (i > 3 && substr(\$i, 1, 1) == substr(\$(i - 1), 1, 1)) exit 1
	}
	next
}
{
	if (NF != 0) exit 1
}"
test "$(./rubik --generate --solve --length 1 --count 1 | wc -l)" -eq 2
test "$(./rubik --generate --solve --length 1 --count 2 | wc -l)" -eq 5

generated=$(./rubik --generate --timed --solve --length 10 --count 2)
test "$(printf '%s\n' "$generated" | wc -l)" -eq 5
mix=$(printf '%s\n' "$generated" | sed -n '1s/^Mix 1: //p')
solution=$(printf '%s\n' "$generated" | sed -n '2s/^Solution 1: //p')
test "$(printf '%s\n' "$mix" | wc -w)" -eq 10
test -z "$(./rubik "$mix $solution")"
mix=$(printf '%s\n' "$generated" | sed -n '4s/^Mix 2: //p')
solution=$(printf '%s\n' "$generated" | sed -n '5s/^Solution 2: //p')
test "$(printf '%s\n' "$mix" | wc -w)" -eq 10
test -z "$(./rubik "$mix $solution")"
if ./rubik --generate --timed >/dev/null 2>&1; then
	exit 1
fi
if ./rubik --generate --solve --timed --timed >/dev/null 2>&1; then
	exit 1
fi

generated=$(./rubik --generate --stats --timed --solve --length 10 --count 2)
printf '%s\n' "$generated" | awk '
NR % 5 == 1 { if ($1 != "Mix" || NF != 12) exit 1; next }
NR % 5 == 2 { if ($1 != "Solution" || NF < 2) exit 1; count = NF - 2; next }
NR % 5 == 3 { if ($1 != "Mouvements:" || NF != 2 || $2 != count) exit 1; next }
NR % 5 == 4 { if ($1 != "Temps:" || $2 !~ /^[0-9]+\.[0-9]+$/ || $3 != "s" || NF != 3) exit 1; next }
{ if (NF != 0) exit 1 }
END { if (NR != 9) exit 1 }'
generated=$(./rubik --generate --solve --stats --length 1 --count 1)
printf '%s\n' "$generated" | grep -qx 'Mouvements: 1'
if ./rubik --generate --stats >/dev/null 2>&1; then
	exit 1
fi
if ./rubik --generate --solve --stats --stats >/dev/null 2>&1; then
	exit 1
fi
