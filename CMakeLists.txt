cmake_minimum_required(VERSION 3.24)
project(Event_Driven_Chat_Server C)

set(CMAKE_C_STANDARD 11)

add_executable(Event_Driven_Chat_Server chatServer.c chatServer.h)
