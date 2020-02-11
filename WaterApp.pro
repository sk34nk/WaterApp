TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle qt

QMAKE_CXXFLAGS += -std=c++11
QMAKE_LFLAGS += -L/usr/local/lib -L/usr/lib64 -lvulkan -lglfw -pthread -lGLEW -lGLU -lGL -lrt -lXrandr -lXxf86vm -lXi -lXinerama -lX11

SOURCES += \
        createApp.cpp \
        main.cpp

HEADERS += \
    createApp.hpp
