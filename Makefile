TARGET1 = ftps
SRCS1 = ftps.c common.c

TARGET2 = ftpc
SRCS2 = ftpc.c common.c

OBJS1 = $(SRCS1:.c=.o)
DEPS1 = $(SRCS1:.c=.d)
OBJS2 = $(SRCS2:.c=.o)
DEPS2 = $(SRCS2:.c=.d)

CPP = gcc
DEBUG = -ggdb
CPPFLAGS = -Wall $(DEBUG)
LFLAGS = 

.PHONY: clean all

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJS1)
	$(CPP) $(CPPFLAGS) $(LFLAGS) $(OBJS1) -o $(TARGET1)
	
$(TARGET2): $(OBJS2)
	$(CPP) $(CPPFLAGS) $(LFLAGS) $(OBJS2) -o $(TARGET2)

%.o: %.c
	$(CPP) $(CPPFLAGS) -c $< -o $@

%.d: %.c
	$(CPP) -MM $(CPPFLAGS) $< > $@

clean:
	rm -f $(OBJS1) $(DEPS1) $(TARGET1) $(OBJS2) $(DEPS2) $(TARGET2)

-include $(DEPS1) $(DEPS2)
