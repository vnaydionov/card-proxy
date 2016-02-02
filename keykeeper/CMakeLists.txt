
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -pedantic-errors")

add_executable (key_keeper
    key_keeper.c util.c conf.c
)

target_link_libraries (
    key_keeper crypto
)

install (TARGETS key_keeper DESTINATION bin)
install (FILES key_keeper.cfg DESTINATION etc/card_proxy)
install (PROGRAMS key_keeper-ping key_keeper-restarter DESTINATION bin)
install (PROGRAMS key_keeper.init DESTINATION etc/init.d RENAME key-keeper)
install (FILES key_keeper.logrotate DESTINATION etc/logrotate.d
         RENAME card_proxy_keykeeper)
