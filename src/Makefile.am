AUTOMAKE_OPTIONS=no-dependencies
bin_PROGRAMS=hsen hosts3d
AM_CPPFLAGS=$(GLFW_CFLAGS)
AM_CXXFLAGS=-std=gnu++11
hsen_SOURCES=llist.cpp misc.cpp proto.cpp hsen.cpp
hsen_LDADD=-lpcap
hosts3d_SOURCES=llist.cpp misc.cpp glwin.cpp objects.cpp hosts3d.cpp
hosts3d_CXXFLAGS=$(AM_CXXFLAGS) -pthread
hosts3d_LDFLAGS=-pthread
hosts3d_LDADD=$(GLFW_LIBS) -lGL -lGLU
