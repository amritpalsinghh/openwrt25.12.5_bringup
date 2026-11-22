# build hellocpp executable when user executes "make"
hellocpp: hellocpp.o
	$(CXX) $(LDFLAGS) hellocpp.o -o hellocpp

hellocpp.o: hellocpp.cpp
	$(CXX) $(CXXLAGS) -c hellocpp.cpp

# remove object files and executable when user executes "make clean"
clean:
	rm *.o hellocpp

