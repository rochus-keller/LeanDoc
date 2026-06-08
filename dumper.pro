QT       += core

QT       -= gui

TARGET = dumper
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

HEADERS += \
    LeanDocAst2.h \
    LeanDocLexer2.h \
    LeanDocParser2.h \
    LeanDocValidator.h

SOURCES += \
    LeanDocAst2.cpp \
    LeanDocLexer2.cpp \
    LeanDocParser2.cpp \
    LeanDocValidator.cpp \
    dumper.cpp
