# pact: processes agreeing to quit at the same time

This program starts any number of other programs. Once any of them exits, it
tells all the others to exit too, by sending `SIGTERM`. (`pact` also tells the
other programs to exit if it recieves any signal, so you can press Ctrl-C or
send it `SIGHUP` or `SIGTERM`.) If any of the programs don't exit within 5
seconds, it then sends the stragglers and itself a `SIGKILL`; otherwise, it
returns the exit status of the first program that exited.

Specify the programs to run by separating them with an empty-string argument.
Also, `pact` strips one leading space off each argument, if there is one. For
example,

```sh
pact ' sleep' ' 1' '' ' sleep' ' 2' '' sleep 3
```

will wait approximately one second before stopping the other `sleep` processes
and returning.

These rules for argument handling are in the same style as
[`execline`][execline] [blocks][], so if you use `pact` in an `execlinenb`
script then you can write the above example like this instead:

```sh
pact { sleep 1 } { sleep 2 } sleep 3
```

[execline]: http://skarnet.org/software/execline/
[blocks]: http://skarnet.org/software/execline/el_semicolon.html

Since the leading spaces are optional, however, you can also just run:

```sh
pact sleep 1 '' sleep 2 '' sleep 3
```
