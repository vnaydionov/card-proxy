#!/bin/sh

SELF="$0"
BIN="$1"
shift 
DELAY=2

cd /var/tmp

function log_info {
    DT=`date '+%Y-%m-%d %H:%M:%S'`
    MSG="$1"
    echo "[$DT] <$SELF> info: $MSG"
}

while [ 1 = 1 ] ; do
    "$BIN" "$@"
    RC=$?
    log_info "Process \"$BIN\" exited with code $RC. Sleep for $DELAY sec before restart."
    sleep "$DELAY"
done

