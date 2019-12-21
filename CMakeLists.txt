cmake_minimum_required(VERSION 3.10)

project(jdedit)

add_executable(jdedit "")

target_sources(jdedit PRIVATE src/append_buffer.c)
target_sources(jdedit PRIVATE src/editor.c)
target_sources(jdedit PRIVATE src/main.c)
target_sources(jdedit PRIVATE src/row.c)
target_sources(jdedit PRIVATE src/syntax.c)
target_sources(jdedit PRIVATE src/terminal.c)