
#include <stdlib.h>
#include <unistd.h>

#include "SescConf.h"
#include "Report.h"

void wattch_setup();
extern int verbose;

const char *getOutputFile(int argc, const char **argv) {

  for(int i=0;i<argc;i++) {
    if (argv[i][0] != '-' || argv[i][1] != 'o')
      continue;

    // -c found
    if (argv[i][2]==0) {
      if (argc == (i+1)) {
        MSG("ERROR: Invalid command line call. Use -o tmp.conf");
        exit(-2);
      }

      return argv[i+1];
    }
    return &argv[i][2];
  }

  return 0;
}

int main(int argc, const char **argv) {

  const char *outfile=getOutputFile(argc, argv);

  if (outfile==0) {
    fprintf(stderr,"Usage:\n\t%s [-v] -c <tmp.conf> -o <power.conf>\n",argv[0]);
    exit(-1);
  }

  if (argv[1][0] == '-' && argv[1][1] == 'v')
    verbose = 1;
  else
    verbose = 0;
  
  //-------------------------------
  SescConf = new SConfig(argc, argv);

  unlink(outfile);
  Report::openFile(outfile);

  fprintf(stderr,"++++++++++++++BEGIN WATCH\n");
  wattch_setup();
  fprintf(stderr,"++++++++++++++END   WATCH\n");

//   fprintf(stderr,"++++++++++++++BEGIN ORION\n");
//  orion_setup(SescConf);
//  fprintf(stderr,"++++++++++++++END   ORION\n");

  // dump the wattchified configuration
  SescConf->dump(true);

  Report::close();
}
