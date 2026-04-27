cmake_minimum_required(VERSION 3.20)
project(vir_tinysh C)

# Add an executable target
add_executable(vir_tinysh
    main.c
    tinysh.c
    tinysh.h
)