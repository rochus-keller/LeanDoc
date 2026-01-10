QT       += core

QT       -= gui

TARGET = LeanDocToTypst
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

HEADERS += \
    LeanDocAst2.h \
    LeanDocLexer2.h \
    LeanDocParser2.h

SOURCES += \
    LeanDocLexer2.cpp \
    LeanDocParser2.cpp \
    dumper.cpp



