#cd back to the top level of the esesc directory.
cd ../

#Get the path of esesc source and build directory
input=$(head -1 cmake_install.cmake)
esescDir=`echo $input| cut -d':' -f 2`
buildDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#Run bash script to set up directories and generate benchmark scripts
sh $esescDir/gold/setup.sh $esescDir


# take a known 'golden-brick' output from ESESC and compare against runs of all 3 
# benchmarks this script will invoke
#
# The script will compare a SLOW and FAST run against the golden brick outputs, as well as do a sanity 
# check that the SLOW configuration is indeed slower than the FAST configuration. 
#
# Check the outputs for correctness by going in to the compare directory and viewing the 
# SLOW.last:SLOW.gold
# FAST.last:FAST.gold
# SLOW.last:FAST.last


#run ESESC to get new output to compare against golden-brick
sh $esescDir/gold/exe/maketable.sh SLOW NULL $buildDir $esescDir
sh $esescDir/gold/exe/maketable.sh FAST NULL $buildDir $esescDir

#now call cmptab-eq(equal) to compare the new outputs against the golden brick o
#be equal to the golden brick output
cd $esescDir/gold/tables
sh cmptab-eq.sh SLOW.last SLOW.gold $esescDir
sh cmptab-eq.sh FAST.last FAST.gold $esescDir

#now call cmptab-ne.sh for the sanity check of the SLOW vs. FAST conf. These out
#rather should be LT or GT
sh cmptab-ne.sh SLOW.last FAST.last $esescDir

#copy all output files to the output directory
mv SLOW.last:SLOW.gold $buildDir/regression/output
mv FAST.last:FAST.gold $buildDir/regression/output
mv SLOW.last:FAST.last $buildDir/regression/output

#look for ERROR and OK and return message
cd $buildDir/regression/output
wordcount=`grep ERROR SLOW.last:SLOW.gold FAST.last:FAST.gold SLOW.last:FAST.last | wc -w`
if [ $wordcount != 0 ]; then
  echo "\n"
  echo "\n"
  echo "\n--------------------------ERRORS FOUND, EXAMINE $buildDir/regression/output------"
else
  echo "\n"
  echo "\n"
  echo "\n-------------------------------------------ALL OK---------------------"
fi 

