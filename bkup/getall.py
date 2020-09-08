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
                nflows = fdict["start"]["test_start"]["num_streams"]
                output.write(str(nflows) + "\t")
                output.write(str(fdict["qlen"]) + "\t")
                output.write(str(2*fdict["delay"]) + "\t")
                fct = 60000000 if err else fdict["end"]["sum_received"]["seconds"] * 1e6
                output.write(str(int(fct)) + "\t")
                retransmits = -1 if err else fdict["end"]["sum_sent"]["retransmits"]
                #output.write(str(fdict["reorder"]) + "\t")
                output.write(str(retransmits) + "\t")
                output.write("-1\t")
                output.write("-1\t")
                meanrtt = -1 if err else sum(s["sender"]["mean_rtt"] for s in fdict["end"]["streams"]) / nflows
                output.write(str(meanrtt) + "\n")
