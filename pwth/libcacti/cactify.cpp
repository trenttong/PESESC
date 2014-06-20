
#include <stdlib.h>
#include <unistd.h>

#include "SescConf.h"
#include "Report.h"

void cacti_setup();

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

  //-------------------------------
  // Look for output file
  const char *outfile=getOutputFile(argc, argv);

  if (outfile==0) {
    fprintf(stderr,"Usage:\n\t%s -c <tmp.conf> -o <power.conf>\n",argv[0]);
    exit(-1);
  }

  //-------------------------------

  SescConf = new SConfig(argc, argv);

  unlink(outfile);
  Report::openFile(outfile);

  fprintf(stderr,"++++++++++++++BEGIN CACTI\n");
  cacti_setup();
  fprintf(stderr,"++++++++++++++END   CACTI\n");

  // dump the cactify configuration
  SescConf->dump(true);

  Report::close();
}
