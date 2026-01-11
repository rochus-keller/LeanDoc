QT       += core

QT       -= gui

TARGET = LeanDocToTypst
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

HEADERS += \
    LeanDocAst2.h \
    LeanDocLexer2.h \
    LeanDocParser2.h \
    LeanDocTypstGen.h

SOURCES += \
    LeanDocAst2.cpp \
    LeanDocLexer2.cpp \
    LeanDocParser2.cpp \
    LeanDocTypstGen.cpp \
    leandoc2typst.cpp



