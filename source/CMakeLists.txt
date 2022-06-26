set(CGRAPHICS_HEADERS 
		cameras/camera2d.hpp
        cameras/camera3d.hpp
        cameras/cameraFirstPerson.hpp
        shaders/shader.hpp
        performanceMonitor.hpp
        mesh.hpp
        model.hpp
		)
set(CGRAPHICS_SOURCES
		shaders/shader.cpp
		)

add_library(cgraphics STATIC ${CGRAPHICS_SOURCES} ${CGRAPHICS_HEADERS} cgraphics.hpp ${Shaders})
if (MSVC)
    target_compile_options(cgraphics PUBLIC /wd5033)
endif(MSVC)
target_include_directories(cgraphics PRIVATE ${LIBS_INCLUDE_DIRECTORIES} CGRAPHICS_INCLUDE_DIRECTORY)
target_link_libraries(cgraphics PRIVATE ${LIBS_LIBRARIES})
set_property(TARGET cgraphics PROPERTY CXX_STANDARD 20)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CGRAPHICS_SOURCES} ${CGRAPHICS_HEADERS})