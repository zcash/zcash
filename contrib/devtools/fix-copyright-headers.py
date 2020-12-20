#!/usr/bin/env python3
'''
Run this script inside of src/ and it will look for all the files
containing a line like this

// Copyright (c) 2016-2018 The Zcash developers

or

// Copyright (c) 2016 The Zcash developers

and try to find the latest update from the git. If the file is
updated during the current year (say 2020), the script will chagne the above line to:

// Copyright (c) 2016-2020 The Zcash developers

Original Author: @gubatron
Updated by: @jadijadi
'''

import os
import time

year = str(time.gmtime()[0]) # current year
replace_command = ("sed -E -i 's/Copyright \(c\) [0-9\-]+ The Zcash developers"
                   "/Copyright (c) %s The Zcash developers/' '%s'")
find_files_command = "find . -iname '*%s'"
find_first_copyright_from_file = ("grep -oP 'Copyright \(c\) "
                               "\K\d{4}(?=(-\d{4})* The Zcash developers)' %s")

extensions = [".cpp", ".h", ".hpp"]

def get_last_git_modified(file_path):
    git_log_command = "git log " + file_path +" | grep Date | head -1"
    command = os.popen(git_log_command)
    output = command.read().replace("\n", "")
    return output

n = 0
for extension in extensions:
    all_files = os.popen(find_files_command % extension)
    for file_path in all_files:
        file_path = file_path[1:-1]
        if file_path.endswith(extension):
            file_path = os.getcwd() + file_path
            last_modification_year = get_last_git_modified(file_path)
            if str(year) in last_modification_year:
                print(n, file_path, ", Last modification: ", last_modification_year)

                # find first modification from the file itself
                output = os.popen(find_first_copyright_from_file % file_path)
                start_year = output.read().replace("\n", "")
                if start_year == year:
                    show_year = year
                else:
                    show_year = f"{start_year}-{year}"
                os.popen(replace_command % ( show_year, file_path))
                n = n + 1
