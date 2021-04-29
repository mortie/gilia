#!/bin/sh

set -e

if [ -z "$1" ]; then
	echo "Usage: $0 <trace>"
	exit 1
fi

cat "$1" | while read line; do
	if printf "%s\n" "$line" | grep -P "\d+\s+0x[a-z0-9]+" >/dev/null; then
		path="$(printf "%s\n" "$line" | sed 's/.*(//; s/+.*)//')"
		addr="$(printf "%s\n" "$line" | sed 's/.*+//; s/)//')"
		addrline="$(addr2line -f -e "$path" "$addr" | tr '\n' ' ')"
		printf "%s\n" "$line" | sed "s#(.*)#( $addrline)#"
	else
		echo "$line"
	fi
done
