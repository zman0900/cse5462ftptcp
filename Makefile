TARGET1 = ftps
SRCS1 = ftps.c common.c tcpd_interface.c

TARGET2 = ftpc
SRCS2 = ftpc.c common.c tcpd_interface.c

TARGET3 = tcpd
SRCS3 = tcpd.c common.c tcpheader.c

TARGET4 = timer
SRCS4 = timer.c common.c

OBJS1 = $(SRCS1:.c=.o)
DEPS1 = $(SRCS1:.c=.d)
OBJS2 = $(SRCS2:.c=.o)
DEPS2 = $(SRCS2:.c=.d)
OBJS3 = $(SRCS3:.c=.o)
DEPS3 = $(SRCS3:.c=.d)
OBJS4 = $(SRCS4:.c=.o)
DEPS4 = $(SRCS4:.c=.d)

CPP = gcc
DEBUG = -ggdb
CPPFLAGS = -Wall $(DEBUG)
LFLAGS = 

.PHONY: clean all

all: $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4)

$(TARGET1): $(OBJS1)
	$(CPP) $(CPPFLAGS) $(LFLAGS) $(OBJS1) -o $(TARGET1)
	
$(TARGET2): $(OBJS2)
	$(CPP) $(CPPFLAGS) $(LFLAGS) $(OBJS2) -o $(TARGET2)
	
$(TARGET3): $(OBJS3)
	$(CPP) $(CPPFLAGS) $(LFLAGS) $(OBJS3) -o $(TARGET3)

$(TARGET4): $(OBJS4)
	$(CPP) $(CPPFLAGS) $(LFLAGS) $(OBJS4) -o $(TARGET4)

%.o: %.c
	$(CPP) $(CPPFLAGS) -c $< -o $@

%.d: %.c
	$(CPP) -MM $(CPPFLAGS) $< > $@

clean:
	rm -f $(OBJS1) $(DEPS1) $(TARGET1) $(OBJS2) $(DEPS2) $(TARGET2) $(OBJS3) \
	      $(DEPS3) $(TARGET3) $(OBJS4) $(DEPS4) $(TARGET4)

-include $(DEPS1) $(DEPS2) $(DEPS3) $(DEPS4)
