QT += core gui widgets
CONFIG += plugin c++11
TEMPLATE = lib
TARGET = capture_tools_plugin
DESTDIR = ../../build/plugins

INCLUDEPATH += ../../debug_workbench

SOURCES += capture_tools_plugin.cpp

HEADERS += \
    capture_tools_plugin.h \
    ../../debug_workbench/idebug_tool_plugin.h
