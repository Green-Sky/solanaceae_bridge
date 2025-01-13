cmake_minimum_required(VERSION 3.14...3.24 FATAL_ERROR)

add_library(solanaceae_bridge STATIC
	./bridge.hpp
	./bridge.cpp
)

target_compile_features(solanaceae_bridge PUBLIC cxx_std_17)
target_link_libraries(solanaceae_bridge PUBLIC
	solanaceae_contact
	solanaceae_message3
)

