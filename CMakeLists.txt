cmake_minimum_required(VERSION 3.20)
project(rog-ally-menu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick)

qt_add_executable(rog-ally-menu
    src/main.cpp
    src/backend.cpp
)

qt_add_resources(rog-ally-menu "qml"
    PREFIX "/"
    FILES qml/Main.qml
)

target_link_libraries(rog-ally-menu PRIVATE Qt6::Core Qt6::Quick)

install(TARGETS rog-ally-menu DESTINATION bin)
install(PROGRAMS rog-ally-menu-helper DESTINATION bin)
install(FILES rog-ally-menu.service DESTINATION lib/systemd/user)
install(FILES 99-rog-ally-menu.rules DESTINATION lib/udev/rules.d)
install(FILES com.rogallymenu.pkexec.policy DESTINATION share/polkit-1/actions)
