import os
import json

pdata = "pdata"

with open(pdata, "w") as output:
    for fname in os.listdir(os.getcwd()):
        if fname[-4:] == "json":
            print(fname)
            with open(fname, "r") as f:
                err = False
                fdict = json.loads(f.read())
                if "error" in fdict.keys():
                    err = True
                output.write(fname[4:-5] + "\t")
                output.write(str(fdict["bw_freq"]) + "\t")
                nflows = fdict["nflows"]
                output.write(str(nflows) + "\t")
                output.write(str(fdict["qlen"]) + "\t")
                output.write(str(2*fdict["delay"]) + "\t")
                fct = 60000000 if len(fdict["streams"]) != nflows else max([s["fct"] for s in fdict["streams"]])
                output.write(str(int(fct)) + "\t")
                retransmits = -1
                #output.write(str(fdict["reorder"]) + "\t")
                output.write(str(retransmits) + "\t")
                output.write("-1\t")
                output.write("-1\t")
                meanrtt = -1
                output.write(str(meanrtt) + "\t")
                output.write(str(fdict["pre"]) + "\n")
