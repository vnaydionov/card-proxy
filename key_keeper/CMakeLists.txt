
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -pedantic-errors")

add_executable (key_keeper
    key_keeper.c util.c conf.c
)

target_link_libraries (
    key_keeper crypto
)

install (TARGETS key_keeper DESTINATION bin)
install (FILES key_keeper.cfg DESTINATION etc/card_proxy)
install (PROGRAMS key_keeper-ping key_keeper_restarter.sh DESTINATION bin)
#install (PROGRAMS key-keeper DESTINATION etc/init.d)
#install (FILES key-keeper.logrotate DESTINATION etc/logrotate.d)
