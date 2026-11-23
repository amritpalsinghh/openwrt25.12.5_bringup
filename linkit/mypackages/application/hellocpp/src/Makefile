all: hellocpp

hellocpp: hellocpp.o
	$(CXX) $(LDFLAGS) hellocpp.o -o hellocpp

hellocpp.o: hellocpp.cpp
	$(CXX) $(CXXFLAGS) -c -o hellocpp.o hellocpp.cpp

clean:
	rm -f hellocpp hellocpp.o

.PHONY: all clean
