QT += core gui widgets
CONFIG += plugin c++11
TEMPLATE = lib
TARGET = system_monitor_plugin
DESTDIR = ../../build/plugins

INCLUDEPATH += ../../debug_workbench

SOURCES += system_monitor_plugin.cpp

HEADERS += \
    system_monitor_plugin.h \
    ../../debug_workbench/idebug_tool_plugin.h
