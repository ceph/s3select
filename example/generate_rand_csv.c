#include <stdio.h>
#include <stdlib.h>


int main(int argc, char** argv)
{
  if (argc<3)
  {
    printf("%s <num-of-rows> <num-of-columns>\n", argv[0]);
    return -1;
  }

  srand(1234);
  for(int i=0; i<atoi(argv[1]); i++)
  {
    printf("%d,", i);
    for(int y=0; y<atoi(argv[2]); y++)
    {
      printf("%d,", rand()%1000);
    }
    printf("\n");
  }




}
