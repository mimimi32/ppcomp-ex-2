CC = nvcc
CFLAGS = -O3 -g
LDFLAGS = -g
LIBS = 

APP = pi
OBJS = $(APP).o

all: $(APP)

$(APP): $(OBJS)
	$(CC) $^ $(LIBS) -o $@ $(LDFLAGS)

%.o : %.cu
	$(CC) $(CFLAGS) -c $< -o $*.o

clean:
	rm -f *.o
	rm -f $(APP)
	rm -f *~
