cmake_minimum_required(VERSION 3.10)
project(iskra_reader CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(iskra_reader ISKRA_read_new.cpp)

# Koppel tegen Paho MQTT C (en pthreads indien nodig)
target_link_libraries(iskra_reader
    paho-mqtt3c
    paho-mqtt3a
    pthread
)
