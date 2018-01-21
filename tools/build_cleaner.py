import os

if "BAZEL_CMD" not in os.environ:
    # This script is designed to run from within build_cleaner.sh
    #
    # It can't be run from within bazel run because it needs to invoke Bazel
    # which isn't possible because of the global bazel lock.
    raise Exception("No BAZEL_CMD environment variable found. Was the script run with `tools/build_cleaner.sh`?")

import re
import subprocess
import xml.etree.ElementTree as ET
from redbaron import RedBaron

def get_target_tags(root):
    """Get a dict of all target names to their XML tag in the input data."""
    tags = {}
    for child in root:
        tags[child.attrib["name"]] = child
    return tags

def should_process_target(target_tags, name):
    """Returns True if the target is one that should be cleaned."""
    tag = target_tags[name]
    return (name.startswith("//src/") or name.startswith("//src:")) and tag.tag == 'rule'

def extract_label_list(target_tag, name):
    """For a given target XML tag and a name, for example 'deps' or 'srcs',
    extract the list of labels in that list."""
    for child in target_tag:
        if child.tag == "list" and child.attrib["name"] == name:
            return [c.attrib["value"] for c in child]
    return []  # Not found

def extract_rule_string(target_tag, name):
    """For a given target XML tag and a name, for example 'deps' or 'srcs',
    extract the list of labels in that list."""
    for child in target_tag:
        if child.tag == "string" and child.attrib["name"] == name:
            return child.attrib["value"]
    return ""  # Not found

def extract_rule_boolean(target_tag, name):
    """For a given target XML tag and a name, for example 'deps' or 'srcs',
    extract the list of labels in that list."""
    for child in target_tag:
        if child.tag == "boolean" and child.attrib["name"] == name:
            return child.attrib["value"] == "true"
    return False  # Not found

def get_kept_rules(target_tags):
    res = set()
    for target in target_tags:
        target_tag = target_tags[target]
        keep = "buildcleaner-keep" in extract_label_list(target_tag, "tags")
        alwayslink = extract_rule_boolean(target_tag, "alwayslink")
        if keep or alwayslink:
            res.add(target)
    return res

def get_rule_input_labels(target_tag):
    """Given a target XML tag, get a list of labels for the files that are
    directly included in that target."""
    return list(set(
        extract_label_list(target_tag, "srcs") + \
        extract_label_list(target_tag, "textual_hdrs") + \
        extract_label_list(target_tag, "hdrs")))

def rule_headers_labels(target_tag):
    """Given a target XML tag, get a list of labels for the files that are
    directly included in that target."""
    return list(set(
        extract_label_list(target_tag, "textual_hdrs") + \
        extract_label_list(target_tag, "hdrs")))

def get_rules_input_labels(target_tags):
    """Like get_rule_input_labels, but for a dict of targets instead of just one
    target."""
    res = {}
    for name in target_tags:
        res[name] = get_rule_input_labels(target_tags[name])
    return res

def path_label_to_bazelpath(label):
    """Converts a Bazel "label" referring to a file, for example
    //src/secp256k1:src/hash.h to one that is suitable when doing header path
    searches, for example //src/secp256k1/src/hash.h"""
    return label.replace(":", "/").replace("///", "//")

def get_rule_hdr_bazelpaths(target_tags):
    """Returns a dict from Bazel paths (for example //src/a.h or @boost//a/b.h)
    to the names of the rules that export that header."""
    res = {}
    for name in target_tags:
        target_tag = target_tags[name]
        for hdr in rule_headers_labels(target_tag):
            bazel_path = path_label_to_bazelpath(hdr)
            if bazel_path not in res:
                res[bazel_path] = set()
            res[bazel_path].add(name)
    return res

def get_workspace_dir():
    # Use os.environ['WORKING_DIRECTORY'] as supplied by the build_cleaner.sh
    # wrapper script; the actual working directory is clobbered by Bazel.
    workspace_dir = os.environ['WORKING_DIRECTORY']
    while workspace_dir != os.path.dirname(workspace_dir):
        if os.path.isfile(os.path.join(workspace_dir, "WORKSPACE")):
            return workspace_dir
        workspace_dir = os.path.dirname(workspace_dir)
    raise Exception("It seems like this script is not run within a Bazel workspace")

def label_to_path(workspace_dir, label):
    if not label.startswith('//'):
        raise Exception("%s is not an absolute label", label)
    return os.path.join(workspace_dir, label[2::].replace(':', '/'))

include_regex = re.compile(r'^\s*#\s*[Ii][Nn][Cc][Ll][Uu][Dd][Ee]\s*[<"](.*)[>"]')
def extract_nonsystem_includes(path):
    """Return a set of paths (relative to whatever the header search paths are)
    that are #include-d by the file at the given path."""
    res = set()
    if not os.path.isfile(path):
        # This could be the output of a genrule. In that case we simply assume
        # that the genrule has properly declared dependencies and ignore this.
        return res

    with open(path, "r") as f:
        for line in f.readlines():
            match = re.match(include_regex, line)
            if match:
                res.add(match.group(1))
        return res

def empty_if_just_dot(path):
    if path == ".":
        return ""
    elif path.endswith('/') and not path == "/":
        return path[:-1]
    else:
        return path

class HeaderSearchPath:
    """Represents a header search path exported by a Bazel rule. In most cases
    the path is simply a Bazel path, for example @libsodium//include, but if
    the package has an include_prefix it holds that separately as well."""

    def __init__(self, search_path, strip_include_prefix = None, include_prefix = None):
        # search_path must be a normalized path with no . or .. components and
        # no unnecessary double slashes etc.
        if strip_include_prefix and not search_path.endswith("/" + strip_include_prefix):
            raise Exception(
                "Search path %s does not start with strip_include_prefix %s" % (search_path, strip_include_prefix))

        self.search_path = search_path
        self.strip_include_prefix = strip_include_prefix
        self.include_prefix = include_prefix

    def __eq__(self, another):
        return \
            hasattr(another, 'search_path') and self.search_path == another.search_path and \
            hasattr(another, 'strip_include_prefix') and self.strip_include_prefix == another.strip_include_prefix and \
            hasattr(another, 'include_prefix') and self.include_prefix == another.include_prefix

    def __hash__(self):
        return hash("%s|%s|%s" % (self.search_path, self.strip_include_prefix, self.include_prefix))

    def resolve(self, path):
        """Takes a dict of Bazel paths of all header files in the project and a
        path from an #include statement and returns the Bazel path that the file
        would be at if it exists."""

        if self.include_prefix:
            to_strip = self.include_prefix + "/"
            if not path.startswith(to_strip):
                return None  # Not found
            processed_path = path[len(to_strip):]
        else:
            processed_path = path

        return ("%s/%s" % (self.search_path, processed_path)).replace("///", "//")

def get_header_search_paths(target_tags):
    """Returns a dict of target name to a set of header search Bazel-paths (for
    example //src/abc or @boost//) that are used when compiling files in that
    target."""

    # This is configured in tools/bazel.rc, which this script doesn't see
    builtin_search_paths = [HeaderSearchPath("//src")]

    res = {}
    def process_target(name):
        target_tag = target_tags[name]
        if name in res or target_tag.tag != "rule":
            return
        include_prefix = extract_rule_string(target_tag, "include_prefix") or None
        strip_include_prefix = extract_rule_string(target_tag, "strip_include_prefix") or None
        target_workspacename = name.split("//")[0]
        target_dirname = name.split(":")[0]
        if not target_dirname.endswith("/"):
            target_dirname += "/"
        # TODO(per-gron): It is not correct to use a set here because it is
        # unordered. Header search paths are ordered by priority.
        paths = set(builtin_search_paths + [HeaderSearchPath(target_workspacename + "//")])
        for dir in extract_label_list(target_tag, "includes"):
            include_path = target_dirname + empty_if_just_dot(dir)
            if include_path.endswith("/"):
                include_path = include_path[:-1]
            paths.add(HeaderSearchPath(include_path, strip_include_prefix, include_prefix))
        for copt in extract_label_list(target_tag, "copts"):
            if not copt.startswith("-I"):
                continue
            path = copt[2:]
            path = re.sub(r'^\$\(GENDIR\)\/', '', path)
            if path.startswith('external/'):
                paths.add(HeaderSearchPath(re.sub(r'^external\/([^\/]+)\/', r'@\1//', path)))
            else:
                paths.add(HeaderSearchPath('//' + path))
        for dep in extract_label_list(target_tag, "deps"):
            process_target(dep)
            paths = paths.union(res[dep])
        res[name] = paths
    for name in target_tags:
        process_target(name)
    return res

def get_rule_srcs_bazelpaths(target_tag, target_name):
    return [path_label_to_bazelpath(lbl) for lbl in extract_label_list(target_tag, "srcs")]

def resolve_included_headers(workspace_dir, rule_input_labels, src_bazelpaths, hdr_bazelpaths, target_name, search_paths):
    """Given a build rule and its search paths, go through all the headers that
    are included by that rule and find the rules that export that header.
    Returns a dict of header that was included to a set of packages that export
    that header (there can be more than one)."""
    res = {}

    def add_to_res(resolved_path):
        if resolved_path in src_bazelpaths:
            # Including header in the local target adds no dependency.
            return

        if resolved_path not in res:
            res[resolved_path] = set()
        res[resolved_path] = res[resolved_path].union(hdr_bazelpaths[resolved_path])

    for input_label in rule_input_labels[target_name]:
        for path in extract_nonsystem_includes(label_to_path(workspace_dir, input_label)):
            # First check if the header search path is relative to the file path
            bazelpath = re.sub(r"/[^\/]*$", r"/", input_label.replace(":", "/")) + path
            if bazelpath in src_bazelpaths:
                # Including header in the local target adds no dependency. This
                # check is also done in add_to_res so is not strictly needed
                # here but this avoids unnecessary work.
                pass
            elif bazelpath in hdr_bazelpaths:
                add_to_res(bazelpath)
            else:
                for search_path in search_paths:
                    resolved_path = search_path.resolve(path)
                    if resolved_path in hdr_bazelpaths:
                        add_to_res(resolved_path)
                        break

            # If the path is still not found, it could be that it's referring
            # to a header in the srcs section of the current library. These are
            # unimportant to this script and can be ignored.

    # Remove dependencies to self
    for header in res.keys():
        if target_name in res[header]:
            del res[header]

    return res

def real_dependencies(resolved_headers):
    """Takes the return value of resolve_included_headers and computes a set of
    dependencies that the rule should have."""
    deps = set()
    for header in resolved_headers:
        rules = resolved_headers[header]
        if len(rules) != 1:
            raise Exception("Header %s is exported by multiple rules: %s. This script does not support that." % (header, list(rules)))
        deps = deps.union(rules)
    return deps

def kept_deps(kept_rules, target_tag):
    """Takes the the set of kept rules (the ones that build_cleaner has been
    told to not remove even if not used by #includes) and a target's tag and
    returns the intersection of the kept rules and the target_tag's deps."""
    res = set()
    for dep in extract_label_list(target_tag, "deps"):
        if dep in kept_rules:
            res.add(dep)
    return res

def prettify_dependencies(target_name, deps):
    """Takes a set of absolute Bazel rule paths and converts it into a sorted
    list of dependencies, of which some are potentially relative."""
    [target_pkg, target_rulename] = target_name.split(":")

    res = []
    for dep in deps:
        [dep_pkg, dep_rulename] = dep.split(":")
        pkg_name = dep_pkg.split("/")[-1]
        if dep_pkg == target_pkg:
            res.append(":" + dep_rulename)
        elif pkg_name == dep_rulename:
            res.append(dep_pkg)
        else:
            res.append(dep)

    def sort_key(val):
        # Ensure : is treated as smaller than /
        processed_val = val.lower().replace(":", "!")
        # Ensure relative rules are sorted before the others.
        if val[0] == ":":
            return "0" + processed_val
        else:
            return "1" + processed_val

    return sorted(res, key = sort_key)

def calculate_target_deps(workspace_dir, kept_rules, rule_input_labels, rule_src_bazelpaths, rule_hdr_bazelpaths, target_search_paths, target_tag, name):
    search_paths = target_search_paths[name]
    resolved_headers = resolve_included_headers(workspace_dir, rule_input_labels, rule_src_bazelpaths, rule_hdr_bazelpaths, name, search_paths)
    deps = real_dependencies(resolved_headers).union(kept_deps(kept_rules, target_tag))
    return prettify_dependencies(name, deps)

def get_packages_to_process(target_names_to_process):
    """Given a list of Bazel rule names, for example //src:a, //src:b, //src/a:b
    return a list of the Bazel packages, in this case, //src and //src/a"""
    pkgs = set()
    for name in target_names_to_process:
        [pkg, rule] = name.split(":")
        pkgs.add(pkg)
    return pkgs

def get_named_parameter(call_node, name):
    """Given a RedBaron CallNode, find a given named parameter
    CallArgumentNode."""
    for arg_node in call_node:
        if arg_node.target.value == name:
            return arg_node

def replace_deps(build_file_path, package, new_deps_lists):
    """Given a Bazel BUILD file path, its package name and a dict of absolute
    rule names to their dependencies, update the deps in the BUILD file."""
    supported_rule_types = set(["cc_binary", "cc_library", "cc_test"])
    with open(build_file_path, "r") as source_code:
        build_file = RedBaron(source_code.read())

    for invocation_node in build_file:
        if invocation_node.type != "atomtrailers":
            continue
        name_node = invocation_node[0]
        if name_node.type != "name" or name_node.value not in supported_rule_types:
            continue
        call_node = invocation_node[1]
        if call_node.type != "call":
            continue
        name_node = get_named_parameter(call_node, "name")
        if not name_node:
            continue
        name = eval(name_node.value.value)
        deps = get_named_parameter(call_node, "deps")

        absolute_rule_name = "%s:%s" % (package, name)
        if absolute_rule_name not in new_deps_lists:
            # Only edit rules that we have info about
            continue
        new_deps_list = new_deps_lists[absolute_rule_name]
        new_deps_list_syntax = "[\n%s\n    ]" % "\n".join(["        \"%s\"," % dep for dep in new_deps_list])
        if len(new_deps_list) == 0:
            if deps:
                call_node.remove(deps)
        else:
            if deps:
                deps.value = "%s" % new_deps_list_syntax
            else:
                call_node.value.append("deps = %s," % new_deps_list_syntax)

    with open(build_file_path, "w") as source_code:
        source_code.write(build_file.dumps())

class cd:
    def __init__(self, newPath):
        self.newPath = newPath

    def __enter__(self):
        self.savedPath = os.getcwd()
        os.chdir(self.newPath)

    def __exit__(self, etype, value, traceback):
        os.chdir(self.savedPath)

"""Path of whatever is called // in the BUILD files"""
workspace_dir = get_workspace_dir()

with cd(workspace_dir):
    deps_xml = subprocess.check_output([
        os.environ["BAZEL_CMD"],
        "query",
        "--output",
        "xml",
        "--xml:default_values",
        "deps(//src/...)",
    ])
root = ET.fromstring(deps_xml)

"""Dict of all target names to their target XML tags."""
target_tags = get_target_tags(root)
"""Target names of rules that have declared themselves as 'buildcleaner-keep'
to force build_cleaner to not remove them. It also includes alwayslink
libraries."""
kept_rules = get_kept_rules(target_tags)
"""Dict of all target names to labels of the directly included files."""
rule_input_labels = get_rules_input_labels(target_tags)
"""Dict of all Bazel paths to a set of the the target name(s) that exports them."""
rule_hdr_bazelpaths = get_rule_hdr_bazelpaths(target_tags)
"""List of all target names that should be cleaned."""
target_names_to_process = \
    [name for name in target_tags.keys() if should_process_target(target_tags, name)]
"""Dict of all target names to Bazel paths of header search paths."""
target_search_paths = get_header_search_paths(target_tags)
"""Set of all packages that are being processed."""
packages_to_process = get_packages_to_process(target_names_to_process)
"""Dict of (processed) target name to list of deps for that target, prettified
and sorted."""
target_deps = {}
for name in target_names_to_process:
    rule_src_bazelpaths = get_rule_srcs_bazelpaths(target_tags[name], name)
    target_deps[name] = calculate_target_deps(
        workspace_dir,
        kept_rules,
        rule_input_labels,
        rule_src_bazelpaths,
        rule_hdr_bazelpaths,
        target_search_paths,
        target_tags[name],
        name)

for package in packages_to_process:
    build_file = os.path.join(workspace_dir + package, "BUILD")
    replace_deps(build_file, package, target_deps)
