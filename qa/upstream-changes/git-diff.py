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

from subprocess import check_output
changes=check_output(["git", "diff", "cf33f196e79b1e61d6266f8e5190a0c4bfae7224"])

cfiles = re.compile(".*?(\.h|\.cpp|\.c)$") # .cpp, .c, .h

# ifndef ZEROCASH_CHANGES is deliberately prohibited, as it
# is the degenerate case.
IFDEF_ZEROCASH = "#ifdef ZEROCASH_CHANGES"
ELSE_ZEROCASH  = "#else /* ZEROCASH_CHANGES */"
ENDIF_ZEROCASH = "#endif /* ZEROCASH_CHANGES */"

# UPSTREAM = outside of our ifdefs
# OUR_CHANGES = inside of our ifdef
# UPSTREAM_GUARDED = inside of an #else
states = ("UPSTREAM", "OUR_CHANGES", "UPSTREAM_GUARDED", "ERROR")
g = globals()
for i, s in enumerate(states): g[s] = i

# The None key states whether or not arbitrary additions are permitted.
# If additions are permitted, "equal lines" are not.
transition_table = {
    UPSTREAM: { IFDEF_ZEROCASH: OUR_CHANGES, ELSE_ZEROCASH: ERROR, ENDIF_ZEROCASH: ERROR, None: False },
    OUR_CHANGES: { IFDEF_ZEROCASH: ERROR, ELSE_ZEROCASH: UPSTREAM_GUARDED, ENDIF_ZEROCASH: UPSTREAM, None: True },
    UPSTREAM_GUARDED: { IFDEF_ZEROCASH: ERROR, ELSE_ZEROCASH: ERROR, ENDIF_ZEROCASH: UPSTREAM, None: False }
}

for diff in whatthepatch.parse_patch(changes):
    state = UPSTREAM

    # Requirement: Do not rename files from upstream.
    if diff.header.old_path == "/dev/null":
        # We added a file; ignore it.
        continue

    if diff.header.old_path != diff.header.new_path:
        # File was deleted
        if diff.header.new_path == "/dev/null":
            print "A file was removed from upstream code, which is prohibited"
            print "Blame: %s" % diff.header.old_path
        # File was renamed
        else:
            print "A file was renamed from upstream, which is prohibited."
            print "Blame: %s -> %s" % (diff.header.old_path, diff.header.new_path)
        sys.exit(1)

    if cfiles.match(diff.header.old_path):
        def machine(state, file, old_line, new_line, modification):
            if old_line != None and new_line != None:
                # The lines are equal. This is not okay if additions
                # are permitted -- we only want additions, we do not
                # want upstream code to carry over into our guarded
                # modifications.
                if transition_table[state][None]:
                    print "Line from upstream modified without guarding: %s on line %d" % (file, new_line)
                    sys.exit(1)
                
                return state

            if new_line == None:
                # This means the line was removed from upstream code. This is
                # prohibited.
                print "A line was removed from upstream code without guarding."
                print "Blame: %s: %s (old line %d)" % (file, modification, old_line)
                sys.exit(1)

            if modification in transition_table[state]:
                # We can transition.
                return transition_table[state][modification]

            # We must, at this point, be dealing with an addition with respect
            # to upstream. These are only permitted in certain contexts.

            if not transition_table[state][None]:
                # Additions are not allowed here.
                print "Line was added without conditional compilation guarding: %s on line %d" % (file, new_line)
                sys.exit(1)

            return state

        for change in diff.changes:
            state = machine(state, diff.header.old_path, change[0], change[1], change[2])

            #print "State changed to %s after %s" % (state, change)

            if state == ERROR:
                print "Conditional compilation guard failure in %s, not well formed: %s" % (diff.header.old_path, change)
                sys.exit(1)

        if state != UPSTREAM:
            print "Unended #ifdef ZEROCASH_CHANGES in file %s" % diff.header.old_path
            sys.exit(1)
    else:
        print "Change to an unsupported file: %s" % diff.header.old_path
        sys.exit(1)