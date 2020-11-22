CXX = g++
CPPFLAGS = -Wall -O2
CXXFLAGS = 

all: pocsag iir

pocsag: pocsag.cpp
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) $<

iir: iir.cpp
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) $<

.PHONY:clean

clean:
	-rm -f *.o
	-rm -f pocsag
