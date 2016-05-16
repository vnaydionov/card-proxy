#!/bin/sh

for USER in cpr_keykeeper cpr_tokenizer cpr_confpatch cpr_keyapi cpr_service cpr_secvault
do
    if grep -q $USER /etc/passwd
    then
        echo "User $USER exists"
    else
        echo "Create user $USER"
        adduser "$USER"
        mkdir -p /var/log/$USER/ /var/run/$USER/
        chown $USER:$USER /var/log/$USER/ /var/run/$USER/
    fi
done
