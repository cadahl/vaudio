# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

idf_component_register(SRC_DIRS
			"."
			"u8g2"
			"drv"
		       INCLUDE_DIRS 
			".")

idf_build_set_property(C_COMPILE_OPTIONS "-DNDEBUG" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-DU8G2_WITHOUT_FONT_ROTATION" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Wno-format-nonliteral" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Wno-format-security" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Wformat=0" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Werror" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-g" APPEND)
