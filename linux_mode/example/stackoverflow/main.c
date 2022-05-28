#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

typedef struct ABC{
	int a;
	int b;
	int c;
} ABC;

void vuln(char* buffer, int size)
{

	if(!buffer || !size)
		return;
		
	if(size >= sizeof(ABC)){
		ABC* abc = (ABC*)buffer;
		if(abc->a > 12){
			int d;
			memcpy(&d, buffer, size);
		}
	}
}

int main(int argc, char** argv)
{
	int size = 100;
	char buffer[size];

	gets(buffer);

	vuln(buffer, size);

	return 0;
}