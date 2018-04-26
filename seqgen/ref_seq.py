import utils_math as um
import os

class RefSeq():
    def __init__(self, offset):
        self.offset = offset
        self.refs = {
                "lcs": {"ref": [], "args": {}, "f": um.lcs_cost},
                "tail": {"ref": [], "args": {}, "f": um.tail_cost},
                }

    def add_tail(self, seq, maxbt=None, maxskip=None, maxskip_head=None):
        self.refs["tail"]["ref"] = seq
        self.refs["tail"]["args"] = {
                "maxbt": maxbt,
                "maxskip": maxskip,
                "maxskip_head": maxskip_head
                }
        #print("TAIL ref len %d, maxbt %d, maxskip %d, maxskip_head %d" %
        #        (len(seq), maxbt, maxskip, maxskip_head))


    def add_lcs(self, seq, maxbt, maxskip, maxskip_head):
        self.refs["lcs"]["ref"] = seq
        self.refs["lcs"]["args"] = {
                "maxbt": maxbt,
                "maxskip": maxskip,
                "maxskip_head": maxskip_head
                }
        #print("LCS ref len %d, maxbt %d, maxskip %d, maxskip_head %d" %
        #        (len(seq), maxbt, maxskip, maxskip_head))

    def match(self, syscalls, match, skip_cost=[]):
        total_cost = 0
        for name, data in self.refs.items():
            if name in skip_cost:
                continue
            if len(data["ref"]) < 1:
                continue
            cost, skip = data["f"](syscalls, match, data["ref"], **data["args"])
            #if cost != 0:
            #    print("%s: cost %d, skip %d" % (name, cost, skip))
            total_cost += cost
        return total_cost

    def __str__(self):
        r = "OFFSET: %s\n" % self.offset
        r += "LCS ref len %d, maxbt %d, maxskip %d, maxskip_head %d\n" % \
                (len(self.refs["lcs"]["ref"]),
                    self.refs["lcs"]["args"]["maxbt"],
                    self.refs["lcs"]["args"]["maxskip"],
                    self.refs["lcs"]["args"]["maxskip_head"]
                    )
        r += "TAIL ref len %d, maxbt %d, maxskip %d, maxskip_head %d\n" % \
                (len(self.refs["tail"]["ref"]),
                    self.refs["tail"]["args"]["maxbt"],
                    self.refs["tail"]["args"]["maxskip"],
                    self.refs["tail"]["args"]["maxskip_head"]
                    )
        return r

    def to_file(self, fname_base, off_base):
        SC_SEQUENCE_OFF = []
        SC_SEQUENCE_TYP = []
        SC_SEQUENCE = []
        SC_SEQUENCE_ARGS = []
        o = int(self.offset, 16) + off_base
        #enum METRIC_TYPE {FIN_SEQ, EDIT_DIST, LCS_COST, FREQ_COST};
        for name, data in self.refs.items():

            fname_seq = fname_base + "_" + self.offset + "_" + name + ".seq"
            with open(fname_seq, "w") as fd:
                fd.write(" ".join(map(lambda t: "%x" % t, data["ref"])))
            SC_SEQUENCE.append(fname_seq)

            fname_args = fname_base + "_" + self.offset + "_" + name + ".args"
            with open(fname_args, "w") as fd:
                fd.write("%d %d %d" %
                        (data["args"]["maxbt"],
                            data["args"]["maxskip"],
                            data["args"]["maxskip_head"])
                        )
            SC_SEQUENCE_ARGS.append(fname_args)

            if name == "lcs":
                SC_SEQUENCE_TYP.append("2")
            elif name == "tail":
                SC_SEQUENCE_TYP.append("0")

            SC_SEQUENCE_OFF.append("%x" % o)

        return SC_SEQUENCE, SC_SEQUENCE_ARGS, SC_SEQUENCE_TYP, SC_SEQUENCE_OFF
