QT       += core

QT       -= gui

TARGET = leandoc
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

HEADERS += \
    LeanDocAst2.h \
    LeanDocLexer2.h \
    LeanDocParser2.h \
    LeanDocPreprocessor.h \
    LeanDocTypstGen.h \
    LeanDocValidator.h

SOURCES += \
    LeanDoc2Typst.cpp \
    LeanDocAst2.cpp \
    LeanDocLexer2.cpp \
    LeanDocParser2.cpp \
    LeanDocPreprocessor.cpp \
    LeanDocTypstGen.cpp \
    LeanDocValidator.cpp



