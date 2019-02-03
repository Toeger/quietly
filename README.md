# quietly
Remove noise of command line programs while preserving debuggability.

## Motivation
When running command line programs such as builds you want 2 things:
* Minimal output on success
* Extensive debug information on error

Unfortunately many tools do not behave like that and instead spam useless debug information in the absence of an issue to debug.  
`quietly` can mitigate this problem by only printing the output when an error happened.

## Usage
`quietly command` - run command  
`quietly -sh command` - run command in a shell

## Effect
The last line that `command` printed is displayed and replaced whenever it prints another. That way you only see the last line printed. If `command` succeeds (determined by the error code of `command`), the last line is removed and no output shown. If `command` fails, the whole output is printed. `quietly` returns the same error code that `command` returns.

## Examples
`quietly cmake ..`  
`quietly -sh "cmake .. -G Ninja && cmake --build ."`  
Note that `.` and `..` are not valid directories and need to be expanded by a shell first.

## Limitations
* Interactive programs are only partially supported. You can do `quietly rm -R folder` and if there are read-only files `rm` will print "rm: remove write-protected regular file 'folder/something'?" and you can enter "y" and it will be passed to `rm`. However, for programs that print a description and then "Are you sure? [y/n]", such as `apt-get`, you will only see the last line which means you lose context.
* To be able to conditionally print the output of `command`, `quietly` needs to store the output. If `command` prints gigabytes of text, `quietly` will store said gigabytes of text. If `command` spits out more text than you have free RAM, you should not use `quietly` for that `command`.
