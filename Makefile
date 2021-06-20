all : project3
project3:
	gcc  project3.cpp -lstdc++ -o project3 -lm


clean :
	rm -f project3
