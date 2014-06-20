# $1=esesc directory

#Check if environment variable benchmark was provided
if [ "$#" == "0" ]; then
  echo "Error! Something happened with the paths to benchmarks!"
  exit 1
fi

#Check if the directories exist
if [ ! -d "./regression" ]; then
  echo "Created directory ./regression"
  mkdir ./regression
fi

if [ ! -d "regression/run" ]; then
  echo "Created directory ./regression/run"
  mkdir ./regression/run
fi

if [ ! -d "regression/output" ]; then
  echo "Created directory ./regression/output"
  mkdir ./regression/output
fi

#Copy over the files from conf to where the benchmark scripts are
cp $1/conf/* ./regression/run > /dev/null 2>&1

#Fix if user enters relative path. Concat ../../ because we
#cd into 2 directories. So if user enters ../benchsuite at top level
#directory, we need ../../../benchsuite
path=$1/conf

#Run mt-scripts to generate benchmark testing scripts
cd ./regression/run
./mt-scripts.rb -b single_core -e default_user -m SMARTSMode \
              -c esesc.conf \
              -x ../../main/esesc -l -j $path

