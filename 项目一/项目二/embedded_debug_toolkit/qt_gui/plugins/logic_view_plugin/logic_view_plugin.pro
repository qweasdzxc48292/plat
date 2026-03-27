QT += core gui widgets
CONFIG += plugin c++11
TEMPLATE = lib
TARGET = logic_view_plugin
DESTDIR = ../../build/plugins

INCLUDEPATH += ../../debug_workbench

SOURCES += logic_view_plugin.cpp

HEADERS += \
    logic_view_plugin.h \
    ../../debug_workbench/idebug_tool_plugin.h
