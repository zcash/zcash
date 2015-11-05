import whatthepatch
import re
import sys

# This QA check prevents us from accidentally modifying upstream
# code.
#
# * If you make a change to upstream c++ code, your changes must
#   be guarded with '#ifdef ZEROCASH_CHANGES'. It is very picky:
#   you cannot even modify newlines in upstream code.
# * This check will ignore files that we add.
# * This check does not support python files yet, so you cannot
#   modify upstream python files.

# UPSTREAM = outside of our ifdefs
# OUR_CHANGES = inside of our ifdef
# UPSTREAM_GUARDED = inside of an #else
states = ("UPSTREAM", "OUR_CHANGES", "UPSTREAM_GUARDED", "ERROR")
g = globals()
for i, s in enumerate(states): g[s] = i

g["cfiles"] = re.compile(".*?(\.hpp|\.h|\.cpp|\.c)$") # .cpp, .c, .h, .hpp

g["IFDEF_ZEROCASH"] = "#ifndef ZEROCASH_CHANGES"
g["ELSE_ZEROCASH"]  = "#else /* ZEROCASH_CHANGES */"
g["ENDIF_ZEROCASH"] = "#endif /* ZEROCASH_CHANGES */"

# If additions are permitted, "equal lines" are not.
g["transition_table"] = {
    UPSTREAM: { IFDEF_ZEROCASH: UPSTREAM_GUARDED, ELSE_ZEROCASH: ERROR, ENDIF_ZEROCASH: ERROR },
    OUR_CHANGES: { IFDEF_ZEROCASH: ERROR, ELSE_ZEROCASH: ERROR, ENDIF_ZEROCASH: UPSTREAM },
    UPSTREAM_GUARDED: { IFDEF_ZEROCASH: ERROR, ELSE_ZEROCASH: OUR_CHANGES, ENDIF_ZEROCASH: UPSTREAM }
}

# If additions are permitted, "equal lines" are not.
g["allow_additions"] = {
    UPSTREAM: False,
    OUR_CHANGES: True,
    UPSTREAM_GUARDED: False
}

class BrokenPatch(Exception):
    def __init__(self, reason, file, line=0, contents=""):
        self.reason = reason
        self.file = file
        self.line = line
        self.contents = contents
    def __str__(self):
        s = "%s (file = %s, line = %d)" % (self.reason, self.file, self.line)
        if self.contents != "":
            s += ": %s" % self.contents
        return s

def check_valid(changes):
    for diff in whatthepatch.parse_patch(changes):
        state = UPSTREAM

        # Requirement: Do not rename files from upstream.
        if diff.header.old_path == "/dev/null":
            # We added a file; ignore it.
            continue

        if diff.header.old_path != diff.header.new_path:
            raise BrokenPatch("A file was removed from upstream code", diff.header.old_path)

        if cfiles.match(diff.header.old_path):
            def machine(state, file, old_line, new_line, modification):
                if old_line != None and new_line != None:
                    # The lines are equal. This is not okay if additions
                    # are permitted -- we only want additions, we do not
                    # want upstream code to carry over into our guarded
                    # modifications.
                    #if allow_additions[state]:
                    #    raise BrokenPatch("Line from upstream is carried by delta improperly", file, new_line, modification)
                    
                    return state

                if new_line == None:
                    # This means the line was removed from upstream code. This is
                    # prohibited.
                    raise BrokenPatch("Line was removed from upstream", file, old_line, modification)

                if modification in transition_table[state]:
                    # We can transition.
                    return transition_table[state][modification]

                # We must, at this point, be dealing with an addition with respect
                # to upstream. These are only permitted in certain contexts.

                if not allow_additions[state]:
                    # Additions are not allowed here.
                    raise BrokenPatch("Line was added without guarding", file, new_line, modification)

                return state

            for change in diff.changes:
                state = machine(state, diff.header.old_path, change[0], change[1], change[2])

                #print "State changed to %s after %s" % (state, change)

                if state == ERROR:
                    raise BrokenPatch("Not well-formed conditional guards", diff.header.old_path, change[1], change[2])

            if state != UPSTREAM:
                raise BrokenPatch("Unended conditional compilation", diff.header.old_path)
        else:
            raise BrokenPatch("Change to an unsupported file", diff.header.old_path)
            
