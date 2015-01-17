#!/usr/bin/python

import os.path
from collections import deque

SOURCE_EXTS = (".cpp", ".h")
SYMBOL = "ZEROCASH"
DESTINATION_DIR = "../bitcoind"
DEBUG = False

IFDEF    = "#ifdef " + SYMBOL
IFNDEF   = "#ifndef " + SYMBOL
ELSE     = "#else /* " + SYMBOL + " */"
ELSENOT  = "#else /* ! " + SYMBOL + " */"
ENDIF    = "#endif /* " + SYMBOL + " */"
ENDIFNOT = "#endif /* ! " + SYMBOL + " */"

states = ("OUTSIDE", "AFTER", "DISCARD", "ELSE_DISCARD", "KEEP", "ELSE_KEEP", "ERROR")
g = globals()
for i, s in enumerate(states): g[s] = i

transition_matrix = {
    OUTSIDE:      { IFNDEF: KEEP,  IFDEF: DISCARD, ELSE: ERROR,        ELSENOT: ERROR,     ENDIF: ERROR,   ENDIFNOT: ERROR,   None: OUTSIDE      },
    AFTER:        { IFNDEF: KEEP,  IFDEF: DISCARD, ELSE: ERROR,        ELSENOT: ERROR,     ENDIF: ERROR,   ENDIFNOT: ERROR,   None: OUTSIDE      },
    DISCARD:      { IFNDEF: ERROR, IFDEF: ERROR,   ELSE: ERROR,        ELSENOT: ELSE_KEEP, ENDIF: AFTER,   ENDIFNOT: ERROR,   None: DISCARD      },
    ELSE_DISCARD: { IFNDEF: ERROR, IFDEF: ERROR,   ELSE: ERROR,        ELSENOT: ERROR,     ENDIF: OUTSIDE, ENDIFNOT: ERROR,   None: ELSE_DISCARD },
    KEEP:         { IFNDEF: ERROR, IFDEF: ERROR,   ELSE: ELSE_DISCARD, ELSENOT: ERROR,     ENDIF: ERROR,   ENDIFNOT: AFTER,   None: KEEP         },
    ELSE_KEEP:    { IFNDEF: ERROR, IFDEF: ERROR,   ELSE: ERROR,        ELSENOT: ERROR,     ENDIF: ERROR,   ENDIFNOT: OUTSIDE, None: ELSE_KEEP    },
}

def process(source):
    lines = source.splitlines()
    out = deque()
    state = OUTSIDE
    changed = False

    for line in lines:
        transition = transition_matrix[state]
        new_state = transition.get(line, transition[None])
        if DEBUG and (state != OUTSIDE or new_state != OUTSIDE):
            print "%12s->%12s: %s" % (states[state], states[new_state], line)

        if new_state == ERROR:
            raise Exception("could not process %r in state %r" % (line, states[state]))

        if ((new_state == OUTSIDE and state == OUTSIDE) or
            (new_state == OUTSIDE and line not in ("", ENDIF, ENDIFNOT)) or
            (new_state in (KEEP, ELSE_KEEP) and new_state == state)):
            out.append(line)
            if DEBUG and (state != OUTSIDE or new_state != OUTSIDE):
                print "~~~~~~~~~~~~~~~~~~~~~~~~~~"
        else:
            changed = True
        state = new_state

    if state not in (OUTSIDE, AFTER):
        raise Exception("reached end of file in state %r" % (states[state],))

    return (out, changed)

def run():
    for dir_path, subdir_names, file_names in os.walk("."):
        for file_name in file_names:
            path = os.path.join(dir_path, file_name)
            if os.path.splitext(path)[1] in SOURCE_EXTS:
                #if DEBUG: print "Reading %r..." % (path,)
                source = read(path)
                (out, changed) = process(source)
                if changed:
                    out_path = os.path.join(DESTINATION_DIR, path)
                    print "Writing %r..." % (out_path,)
                    out.append("")
                    write(out_path, "\n".join(out))


def read(path):
    rf = open(path, "rb")
    try:
        return rf.read()
    finally:
        rf.close()

def write(path, data):
    wf = open(path, "wb")
    try:
        wf.write(data)
    finally:
        wf.close()

if __name__ == "__main__":
    run()
