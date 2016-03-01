
# a collection of functions for card-proxy service scripts
# NOTE: keep the code 'dash' compatible

CFG_PREFIX="/etc"

log_message () {
    # input vars: LOG_MODE SCRIPT_TAG
    local LEVEL
    local MSG
    local DT
    LEVEL="$1"
    MSG="$2"
    DT=$( date '+%Y-%m-%d %H:%M:%S' )

    if [ "$LOG_MODE" = "stderr" ] ; then
        echo "$DT $LEVEL ${SCRIPT_TAG}[$$]: $MSG" >&2
    else
        if [ "$LOG_MODE" != "none" ] ; then
            # syslog by default
            logger -p "user.$LEVEL" -t "${SCRIPT_TAG}[$$]" "$MSG"
        fi
    fi
}

log_info () {
    # input vars: LOG_MODE SCRIPT_TAG
    log_message "info" "$1"
}

log_error () {
    # input vars: LOG_MODE SCRIPT_TAG
    log_message "error" "$1"
}

detect_env_type () {
    # input vars: LOG_MODE SCRIPT_TAG
    # output vars: ENV_TYPE
    local ENV_CFG
    ENV_CFG="${CFG_PREFIX}/card_proxy_common/environment.cfg.xml"
    ENV_TYPE=$(
        python -c "import xml.etree.ElementTree as et; \
                   print et.parse('$ENV_CFG')\
                       .findtext('Type')" 2> /dev/null
    )
    if [ "$ENV_TYPE" = "" ] ; then
        ENV_TYPE=test
    fi
    log_info "ENV_TYPE=$ENV_TYPE"
}

detect_cfg_file () {
    # input vars: ENV_TYPE SERVANT LOG_MODE SCRIPT_TAG
    # output vars: CFG_FILE
    local CFG_SUFFIX
    CFG_SUFFIX=""
    if [ "$ENV_TYPE" != "prod" ] &&
        [ -r "${CFG_PREFIX}/$SERVANT/$SERVANT.nonprod.cfg.xml" ]
    then
        CFG_SUFFIX=".nonprod"
    fi
    CFG_FILE="${CFG_PREFIX}/$SERVANT/$SERVANT$CFG_SUFFIX.cfg.xml"
    log_info "CFG_FILE=$CFG_FILE"
}

detect_httplistener_port () {
    # input vars: CFG_FILE
    # output vars: HTTP_PORT
    HTTP_PORT=$(
        python -c "import xml.etree.ElementTree as et; \
                   print et.parse('$CFG_FILE')\
                       .findtext('HttpListener/Port')" 2> /dev/null
    )
    log_info "HTTP_PORT=$HTTP_PORT"
}

ping_servant () {
    local HTTP_PORT
    local HTTP_PATH
    HTTP_PORT="$1"
    HTTP_PATH="$2"

    curl -m 1 "http://127.0.0.1:${HTTP_PORT}${HTTP_PATH}" 2> /dev/null |
        grep -q 'success'
    exit $?
}

servant_pinger () {
    local SELF
    local HTTP_PATH
    local PORT_DETECTOR
    SELF="$1"
    PORT_DETECTOR="$2"
    HTTP_PATH="$3"

    SERVANT=$( basename "$SELF" | sed "s/-ping//" )
    SCRIPT_TAG="${SERVANT}-ping"
    log_info "SERVANT=$SERVANT"

    detect_env_type
    detect_cfg_file
    "$PORT_DETECTOR"
    ping_servant "$HTTP_PORT" "$HTTP_PATH"
}

check_alive () {
    # input vars: BIN DELAY SCRIPT_TAG
    local PID
    local IS_ALIVE
    local RC
    PID="$1"
    kill -0 $PID > /dev/null 2> /dev/null
    IS_ALIVE=$?
    if [ $IS_ALIVE != 0 ] ; then
        wait $PID
        RC=$?
        log_error "Process \"$BIN\" ($PID) exited with code $RC. Sleep for $DELAY sec before restart."
        sleep $DELAY
    fi
    return $IS_ALIVE
}

restarter_loop () {
    # input vars: CFG_FILE BIN BIN_PARAMS PINGER DELAY PING_DELAY SCRIPT_TAG
    local PID
    local TS0
    local SPENT
    while [ 1 = 1 ] ; do
        export CONFIG_FILE="$CFG_FILE"
        "$BIN" $BIN_PARAMS &
        PID=$!
        TS0=$(date '+%s')
        while [ 1 = 1 ] ; do
            check_alive $PID || break
            SPENT=$(( $(date '+%s') - $TS0 ))
            if [ $SPENT -lt $PING_DELAY ] ; then
                sleep 1
            else
                TS0=$(date '+%s')
                if ! "$PINGER" ; then
                    check_alive $PID || break
                    log_error "Process \"$BIN\" ($PID) does not respond, trying again in $DELAY sec."
                    sleep $DELAY
                    check_alive $PID || break
                    if ! "$PINGER" ; then
                        check_alive $PID || break
                        log_error "Process \"$BIN\" ($PID) is stalled, killing it."
                        kill -9 $PID > /dev/null 2> /dev/null
                    fi
                fi
            fi
        done
    done
}

servant_restarter () {
    SELF="$1"
    PINGER="$2"
    BIN="$3"
    BIN_PARAMS="$4"

    # input vars: SELF PINGER BIN BIN_PARAMS
    SERVANT=$(basename "$SELF" | sed 's/-restarter//' | sed 's/-/_/')
    SCRIPT_TAG="$SERVANT-restarter"
    DELAY=2
    PING_DELAY=10

    cd /var/tmp

    trap "echo SIGHUP received" HUP

    log_info "BIN=$BIN"
    log_info "PINGER=$PINGER"

    detect_env_type
    detect_cfg_file
    restarter_loop
}

kill_running () {
    # input vars: CP_USER SERVANT
    local SIG
    local RUNNING
    SIG="$1"
    RUNNING=$( ps -Ao user:20,pid,command:100 |
               grep "^$CP_USER .*$SERVANT" |
               grep -v grep | awk '{print$2}' )
    if [ "$RUNNING" != "" ] ; then
        echo "Sending $SIG..."
        kill -$SIG $RUNNING
        return 0
    fi
    return 1
}

servant_reload () {
    # input vars: CP_USER SERVANT
    echo "Reloading..."
    if kill_running HUP ; then
        sleep 3
        echo "Done."
        exit 0
    else
        echo "Not started."
        exit 1
    fi
}

servant_stop () {
    # input vars: CP_USER SERVANT
    echo "Stopping..."
    if kill_running TERM ; then
        sleep 1
        if kill_running KILL ; then
            sleep 1
        fi
    fi
}

servant_start () {
    # input vars: LOG_FILE CP_USER SERVANT
    local RESTARTER
    local PINGER
    local SERVANT_BIN
    local CNT

    echo "Starting..."
    touch "$LOG_FILE"
    chown "$CP_USER:$CP_USER" "$LOG_FILE"
    
    RESTARTER="/usr/bin/${SERVANT}-restarter"
    PINGER="/usr/bin/${SERVANT}-ping"
    SERVANT_BIN="/usr/bin/${SERVANT}"
    su $CP_USER -c "setsid $RESTARTER $PINGER $SERVANT_BIN &>> \"$LOG_FILE\" & "

    # Check if it has started OK
    CNT=15
    while [ $CNT != 0 ] ; do
        if $PINGER ; then
            echo "OK"
            exit 0
        fi
        echo -n "."
        sleep 0.2
        CNT=$(( $CNT - 1 ))
    done
    echo "FAIL"
    servant_stop
    exit 1
}

servant_init () {
    SELF="$1"
    MODE="$2"
    CP_USER="$3"
    if [ "$4" = "RND" ] ; then
        RND_DELAY=Y
    fi

    SERVANT=$(basename "$SELF" | sed 's/-/_/' | sed 's/^[SK][0-9][0-9]//')
    SCRIPT_TAG="${SERVANT}.init"
    LOG_FILE="/var/log/$CP_USER/${SERVANT}.log"

    if [ "$MODE" != "reload" ] && [ "$MODE" != "start" ] &&
        [ "$MODE" != "stop" ] && [ "$MODE" != "restart" ]
    then
        echo "Usage: $SELF start|stop|restart|reload" >&2
        exit 1
    fi
    log_info "SERVANT=$SERVANT"
    log_info "MODE=$MODE"

    if [ "$MODE" = "restart" ] && [ "$RND_DELAY" = "Y" ] ; then
        python -c 'import os,time; d=ord(os.urandom(1)) * 7. / 256; \
                   print "Random delay for %.3f sec" % d; time.sleep(d)'
    fi

    if [ "$MODE" = "reload" ] ; then
        servant_reload
    fi

    if [ "$MODE" = "stop" ] || [ "$MODE" = "restart" ] || [ "$MODE" = "start" ] ; then
        servant_stop
    fi

    if [ "$MODE" = "start" ] || [ "$MODE" = "restart" ] ; then
        servant_start
    fi
}

