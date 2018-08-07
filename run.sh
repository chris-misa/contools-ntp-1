#
# Run the ntp bias measure experiment
#

# Address to ping to
export TARGET_IPV4="10.10.1.2"
export TARGET_IPV6="fd41:98cb:a6ff:5a6a::2"

# Native (local) ping command
export PING_NATIVE_CMD="$(pwd)/iputils/ping"
export NATIVE_DEV="eno1d1"

# Container ping command
export PING_IMAGE_NAME="chrismisa/contools:ping"
export PING_CONTAINER_NAME="ping-container"
export PING_CONTAINER_CMD="docker exec $PING_CONTAINER_NAME ping"
export CONTAINER_DEV="eth0"

# Address of local ntpd
export NATIVE_NTP="10.10.1.1"
# Number of ntp test packets to gather
export NTP_TEST_NUM="10"
# Time between each ntp test packet (in seconds)
export NTP_TEST_INTERVAL="1"

# Argument sequence is an associative array
# between file suffixes and argument strings
declare -A ARG_SEQ=(
  ["i0.5_s56_0"]="-c 5 -i 0.5 -s 56"
)

# Tag for data directory
export DATE_TAG=`date +%Y%m%d%H%M%S`
# File name for metadata
export META_DATA="Metadata"
# Sleep for putting time around measurment
export LITTLE_SLEEP="sleep 3"
export BIG_SLEEP="sleep 10"
# Cosmetics
export B="------------"

# Make a directory for results
echo $B Starting Experiment: creating data directory $B
mkdir $DATE_TAG
cd $DATE_TAG

# Get some basic meta-data
# echo "uname -a -> $(uname -a)" >> $META_DATA
# echo "docker -v -> $(docker -v)" >> $META_DATA
# echo "sudo lshw -> $(sudo lshw)" >> $META_DATA

# Start ping container as service
echo $B Spinning up the ping container $B
docker run -itd --name=$PING_CONTAINER_NAME \
                --entrypoint="/bin/bash" \
                -v $(pwd)/ntpPing:/ntpPing
                $PING_IMAGE_NAME

# Wait for container to be ready
until [ "`docker inspect -f '{{.State.Running}}' $PING_CONTAINER_NAME`" \
        == "true" ]
do
  sleep 1
done


# For each test
for a in ${!ARG_SEQ[@]}
do
  echo $B Test for ping ${ARG_SEQ[$a]} $B
  $BIG_SLEEP
  # Run ntp latency measurment from inside container a couple times
  echo "  Running ntpPing. . ."
  for i in {1..$NTP_TEST_NUM}
  do
    docker exec $PING_CONTAINER_NAME /ntpPing/ntpPing $NATIVE_NTP >> ntp_bais_${a}.txt
    sleep $NTP_TEST_INTERVAL
  done

  # Run native -> target
  $PING_NATIVE_CMD ${ARG_SEQ[$a]} $TARGET_IPV4 > native_${a}.ping

  # Run container -> target
  $PING_CONTAINER_CMD ${ARG_SEQ[$a]} $TARGET_IPV4 > container_${a}.ping

done
