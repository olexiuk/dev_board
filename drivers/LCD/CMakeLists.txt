find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

zephyr_library()
zephyr_include_directories(.)

