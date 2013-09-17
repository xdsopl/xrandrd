#! /bin/bash
# Ahmet Inan <xdsopl@googlemail.com>

XRANDR_QUERY=$(xrandr --query)
CONNECTED=$(echo "$XRANDR_QUERY" | awk '/^.* connected /{ print $1 }')
DISCONNECTED=$(echo "$XRANDR_QUERY" | awk '/^.* disconnected [0-9]*x[0-9]*/{ print $1 }')
COMMON_MODE=$(echo "$XRANDR_QUERY" | awk '/^ *[0-9]*x[0-9]*/{ print $1 }' | sort -n | uniq -d | tail -n1)

ARGS=
for D in $DISCONNECTED ; do
	ARGS="$ARGS --output $D --off"
done

MODE="--auto"
test -z "$COMMON_MODE" || MODE="--mode $COMMON_MODE"

for D in $CONNECTED ; do
	ARGS="$ARGS --output $D $MODE"
done

xrandr $ARGS

