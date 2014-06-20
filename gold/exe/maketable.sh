
#Process.sh
echo begin process.sh
# shell file to run the Ethan Jim CMPE202 ESESC regression class project
# directories of note are:
#    /esesc/gold                run this shell file from here
#    /esesc/gold/config         copies of default and modified shared.conf files live here
#    /esesc/gold/report         reports from report.pl live here including $4/gold/tables/$1.$date.csv file
#    $3/regression/run   esesc shell files run here where the heavy lifting gets done
#    run Process with 'sh Process.sh <shared.conf.(SLOW or FAST or MILD or MEDIUM)>'
#    will output table of /esesc/gold/tables/$1.$date.csv from run in a 9x5 table of benchmarks vs. /esesc/gold/tables/$1.$date.csv for given shared.conf
#    

#$3 = build dir, $4 = esesc dir
#echo $3
#echo $4

# initialize $4/gold/tables/$1.$date.csv output file with current time-stamp
date=`date +%y%m%d-%H%M`

#date >$4/gold/report/$4/gold/tables/$1.$date.csv 


# nested loop through all the test-benchmark combinations
# one test at a time, tests as specified in the corresponding shared.conf files
# for each test run all the benchmarks, generating reports and extracting $4/gold/tables/$1.$date.csv for each test


for BENCHMARK in crafty swim mcf 
do

  # all the ESESC heavy lifting occurs in the ESESC project runs directory
  cd $3/regression/run

  # copy the appropriate shared.conf config file on top of the current working shared.conf file
  cp $4/gold/config/shared.conf.$1 $3/regression/run/shared.conf
  echo -----------------------------------------------------applied shared.conf.$1

  # run one benchmark shell file, ie: crafty _SMARTSMode-run.sh, swim _SMARTSMode-run.sh, ...
  echo -----------------------------------------------------execute $1.$BENCHMARK
  ./$BENCHMARK""_SMARTSMode-run.sh #/dev/null 2>&1 #$4/gold/log/$1.$BENCHMARK.log


  # that generates output in the appropriate sub-directory under $3/regression/run
  # cd to that sub-directory to run a report
  cd $BENCHMARK""_SMARTSMode

  # run the report, directing output over to $4/gold/report for processing soon
  $4/conf/report.pl --last > $4/gold/report/report.$1.$BENCHMARK

  # now we have a report for this test-benchmark combination
  # go to our master report directory to extract the particular $4/gold/tables/$1.$date.csv we're interested in
  cd $4/gold/report


  # now extract the particular $4/gold/tables/$1.$date.csv and comparison we want
done

echo $1, Exetime, BrMis, DL1lat, IPC, uIPC, rawInst, >>$4/gold/tables/$1.$date.csv 

for BENCHMARK in crafty swim mcf
do

  if [ "$BENCHMARK" = crafty ]; then 
    echo -n CRAFTY ,  >>$4/gold/tables/$1.$date.csv	
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Exe -o 1 -c >>$4/gold/tables/$1.$date.csv


    # BRANCH MISPREDICT 
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t MisBr -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK DL1 MEM LAT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Cache -o 24 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK IPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t IPC -o 18 -c >>$4/gold/tables/$1.$date.csv


    # echo report.$1.$BENCHMARK uIPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t uIPC -o 18 -c >>$4/gold/tables/$1.$date.csv

    # echo report.$1.$BENCHMARK rawInst -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t rawInst -o 18 -c >>$4/gold/tables/$1.$date.csv

    echo "" >>$4/gold/tables/$1.$date.csv

  else if [ "$BENCHMARK" = swim ]; then
    echo -n SWIM , >>$4/gold/tables/$1.$date.csv 
    # echo report.$1.$BENCHMARK EXECUTION TIME -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Exe -o 1 -c >>$4/gold/tables/$1.$date.csv


    # echo report.$1.$BENCHMARK BRANCH MISPREDICT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t MisBr -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK DL1 MEM LAT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Cache -o 23 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK IPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t IPC -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK uIPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t uIPC -o 18 -c >>$4/gold/tables/$1.$date.csv

    # echo report.$1.$BENCHMARK rawInst -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t rawInst -o 18 -c >>$4/gold/tables/$1.$date.csv

    echo "" >>$4/gold/tables/$1.$date.csv

  else if [ "$BENCHMARK" = mcf ]; then        
    echo -n MCF , >>$4/gold/tables/$1.$date.csv 
    #echo report.$1.$BENCHMARK EXECUTION TIME -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Exe -o 1 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK BRANCH MISPREDICT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t MisBr -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK DL1 MEM LAT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Cache -o 23 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK IPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t IPC -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK uIPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t uIPC -o 18 -c >>$4/gold/tables/$1.$date.csv

    # echo report.$1.$BENCHMARK rawInst -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t rawInst -o 18 -c >>$4/gold/tables/$1.$date.csv

    echo "" >>$4/gold/tables/$1.$date.csv

  else if [ "$BENCHMARK" = vpr ]; then
    echo -n VPR , >>$4/gold/tables/$1.$date.csv 
    #echo report.$1.$BENCHMARK EXECUTION TIME -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Exe -o 1 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK BRANCH MISPREDICT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t MisBr -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK DL1 MEM LAT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Cache -o 23 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK IPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t IPC -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK uIPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t uIPC -o 18 -c >>$4/gold/tables/$1.$date.csv

    # echo report.$1.$BENCHMARK rawInst -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t rawInst -o 18 -c >>$4/gold/tables/$1.$date.csv

    echo "" >>$4/gold/tables/$1.$date.csv

  else if [ "$BENCHMARK" = gcc ]; then
    echo -n GCC , >>$4/gold/tables/$1.$date.csv 
    #echo report.$1.$BENCHMARK EXECUTION TIME -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Exe -o 1 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK BRANCH MISPREDICT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t MisBr -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK DL1 MEM LAT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Cache -o 24 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK IPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t IPC -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK uIPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t uIPC -o 18 -c >>$4/gold/tables/$1.$date.csv

    # echo report.$1.$BENCHMARK rawInst -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t rawInst -o 18 -c >>$4/gold/tables/$1.$date.csv

    echo "" >>$4/gold/tables/$1.$date.csv

  else if [ "$BENCHMARK" = blackscholes ]; then
    echo -n BLACKSCHOLES , >>$4/gold/tables/$1.$date.csv 
    #echo report.$1.$BENCHMARK EXECUTION TIME -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Exe -o 1 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK BRANCH MISPREDICT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t MisBr -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK DL1 MEM LAT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Cache -o 59 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK IPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t IPC -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK uIPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t uIPC -o 18 -c >>$4/gold/tables/$1.$date.csv

    # echo report.$1.$BENCHMARK rawInst -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t rawInst -o 18 -c >>$4/gold/tables/$1.$date.csv

    echo "" >>$4/gold/tables/$1.$date.csv

  else if [ "$BENCHMARK" = facesim ]; then
    echo -n FACESIM , >>$4/gold/tables/$1.$date.csv 
    #echo report.$1.$BENCHMARK EXECUTION TIME -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Exe -o 1 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK BRANCH MISPREDICT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t MisBr -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK DL1 MEM LAT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Cache -o 53 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK IPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t IPC -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK uIPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t uIPC -o 18 -c >>$4/gold/tables/$1.$date.csv

    # echo report.$1.$BENCHMARK rawInst -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t rawInst -o 18 -c >>$4/gold/tables/$1.$date.csv

    echo "" >>$4/gold/tables/$1.$date.csv

  else if [ "$BENCHMARK" = fft ]; then
    echo -n FFT , >>$4/gold/tables/$1.$date.csv 
    #echo report.$1.$BENCHMARK EXECUTION TIME -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Exe -o 1 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK BRANCH MISPREDICT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t MisBr -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK DL1 MEM LAT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Cache -o 23 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK IPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t IPC -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK uIPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t uIPC -o 18 -c >>$4/gold/tables/$1.$date.csv

    # echo report.$1.$BENCHMARK rawInst -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t rawInst -o 18 -c >>$4/gold/tables/$1.$date.csv

    echo "" >>$4/gold/tables/$1.$date.csv

  else if [ "$BENCHMARK" = ocean ]; then
    echo -n OCEAN , >>$4/gold/tables/$1.$date.csv 
    #echo report.$1.$BENCHMARK EXECUTION TIME -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Exe -o 1 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK BRANCH MISPREDICT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t MisBr -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK DL1 MEM LAT -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t Cache -o 50 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK IPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t IPC -o 18 -c >>$4/gold/tables/$1.$date.csv


    #echo report.$1.$BENCHMARK uIPC -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t uIPC -o 18 -c >>$4/gold/tables/$1.$date.csv

    # echo report.$1.$BENCHMARK rawInst -c >>$4/gold/tables/$1.$date.csv
    $4/gold/exe/tokens -f report.$1.$BENCHMARK -t rawInst -o 18 -c >>$4/gold/tables/$1.$date.csv

    echo "" >>$4/gold/tables/$1.$date.csv

  fi
fi
fi
fi
fi
fi
fi
fi
fi
   done
   cp $4/gold/tables/$1.$date.csv $4/gold/tables/$1.last
   if [ "$2" = gold ]; then
     mv $4/gold/tables/$1.$date.csv $4/gold/tables/$1.gold
     rm $4/gold/tables/$1.last
   fi

