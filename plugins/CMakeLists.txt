cmake_minimum_required(VERSION 3.14...3.24 FATAL_ERROR)

add_library(plugin_bridge SHARED
	./plugin_bridge.cpp
)

target_link_libraries(plugin_bridge PUBLIC
	solanaceae_plugin
	solanaceae_bridge
)

