#-------------------------------------------------
#
# Project created by QtCreator 2017-10-31T18:15:24
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = amb_mon
TEMPLATE = app

INCLUDEPATH += $(BARDYDIR)/BRDINC \
        $(BARDYDIR)/BRDINC/ctrladmpro

win32:LIBS += -l$(BARDYDIR)/lib/brd64
unix:LIBS += -L$(BARDYDIR)/bin -L$(BARDYDIR)/gipcy/lib -lbrd -lgipcy

SOURCES += main.cpp\
        amb_mon.cpp

HEADERS  += amb_mon.h

FORMS    += amb_mon.ui

RESOURCES += \
    amb_mon.qrc
