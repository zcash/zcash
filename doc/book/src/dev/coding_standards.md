Coding Standards
================

Various coding styles have been used during the history of the codebase,
and the result is not very consistent. However, we're now trying to converge to
a single style, so please use it in new code. Old code will be converted
gradually.
- Basic rules specified in src/.clang-format. Use a recent clang-format-3.5 to format automatically.
  - Braces on new lines for namespaces, classes, functions, methods.
  - Braces on the same line for everything else.
  - 4 space indentation (no tabs) for every block except namespaces.
  - No indentation for public/protected/private or for namespaces.
  - No extra spaces inside parenthesis; don't do ( this )
  - No space after function names; one space after if, for and while.

Block style example:
```c++
namespace foo
{
class Class
{
    bool Function(char* psz, int n)
    {
        // Comment summarising what this section of code does
        for (int i = 0; i < n; i++) {
            // When something fails, return early
            if (!Something())
                return false;
            ...
        }

        // Success return is usually at the end
        return true;
    }
}
}
```

Doxygen comments
-----------------

To facilitate the generation of documentation, use doxygen-compatible comment blocks for functions, methods and fields.

For example, to describe a function use:
```c++
/**
 * ... text ...
 * @param[in] arg1    A description
 * @param[in] arg2    Another argument description
 * @pre Precondition for function...
 */
bool function(int arg1, const char *arg2)
```
A complete list of `@xxx` commands can be found at http://www.stack.nl/~dimitri/doxygen/manual/commands.html.
As Doxygen recognizes the comments by the delimiters (`/**` and `*/` in this case), you don't
*need* to provide any commands for a comment to be valid; just a description text is fine.

To describe a class use the same construct above the class definition:
```c++
/**
 * Alerts are for notifying old versions if they become too obsolete and
 * need to upgrade. The message is displayed in the status bar.
 * @see GetWarnings()
 */
class CAlert
{
```

To describe a member or variable use:
```c++
int var; //!< Detailed description after the member
```

or
```cpp
//! Description before the member
int var;
```

Also OK:
```c++
///
/// ... text ...
///
bool function2(int arg1, const char *arg2)
```

Not OK (used plenty in the current source, but not picked up):
```c++
//
// ... text ...
//
```

A full list of comment syntaxes picked up by doxygen can be found at http://www.stack.nl/~dimitri/doxygen/manual/docblocks.html,
but if possible use one of the above styles.


