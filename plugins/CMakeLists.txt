cmake_minimum_required(VERSION 3.14...3.24 FATAL_ERROR)

add_library(plugin_bridge MODULE
	./plugin_bridge.cpp
)

set_target_properties(plugin_bridge PROPERTIES
	C_VISIBILITY_PRESET hidden
)
target_compile_definitions(plugin_bridge PUBLIC ENTT_API_IMPORT)

target_link_libraries(plugin_bridge PUBLIC
	solanaceae_plugin
	solanaceae_bridge
)

