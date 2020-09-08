#!/bin/bash
rm -f fct
touch fct
for i in $( ls ); do
  if [[ $i == "bwlog"* ]]; then
    echo -n "${i: -19}" >> fct
    echo -ne "\t" >> fct
    d0=`sed '1q;d' $i | cut -d, -f1`
    d1=`sed '3q;d' $i | cut -d, -f1`
    echo -n "$((d1-d0))" >> fct
    echo -ne "\t" >> fct
    for j in $( ls ); do
      if [[ $j == "rxlog"* && "${i: -19}" == "${j: -19}" ]]; then
        echo `tail -n 1 $j | cut -d, -f1` >> fct
      fi
    done
  fi
done
