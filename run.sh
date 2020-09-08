#!/bin/bash
c=41
sd=50000
hdl="5us"
dljump=5
bwjump=10
b=10000000000
#declare -a bwarr=(64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576)
declare -a bwarr=(64 90 128 181 256 362 512 724 1024 1448 2048 2896 4096 5792 8192 11585 16384 23170 32768 46340 65536 92681 131072 185363 262144 370727 524288 741455 1048576)
declare -a dlarr=(100000 500000 1000000)
#declare -a dlarr=(50000 100000 200000 500000 1000000)
declare -a flarr=(5 10)
declare -a qlarr=(100)
#declare -a bwarr=(724)
#declare -a flarr=(2)
#declare -a dlarr=(50000)
#declare -a qlarr=(50)
for j in "${flarr[@]}"; do
  for i in "${bwarr[@]}"; do
    for q in "${qlarr[@]}"; do
      for d in "${dlarr[@]}"; do
        n=0
        while [ $(ps u | grep "/home/wew168/ns0b05" | wc -l) -ge $c ]; do
          sleep 5
        done
        bh=$b
        bl=$((b/bwjump))
        dh=$d
        dl=$((d*dljump))
        #dack=`printf %0.0f $(echo "1+($dljump-1)*(1-1/$bwjump)*${dh}*${bh}/1000000000/1500/8" | bc -l)`
        #if [ $j -gt 1 ]; then
        #  dack=`printf %0.0f $(echo "1+${dack}/$j*1.5" | bc -l)`
        #fi
        #if [ $dack -lt 3 ]; then
        #  dack=3
        #fi
        dack=3
        ./waf --run "scratch/tcpltdelay --HostPropDelay=$hdl --Bidir=false --HostRate=40000000000 --MaxBytes=4000000000 --Nsd=$sd --QueueLength=$q --RWND=20000000 --NFlows=$j --Rjitter=10000 --DupAckTh=${dack} --BWP=${bh},${dh}ns,$i,${bl},${dl}ns,$i" &
        sleep 10
      done
    done
  done
done