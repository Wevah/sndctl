# sndctl

v1.2 (2019)  
by Nate Weaver (Wevah)  
https://derailer.org/

A small utility for controlling sound output from the command line on macOS.

```shell
$ sndctl -v <volume>
$ sndctl -b <balance>
```

Volume and balance are from `0.0` to `1.0`, translated to volume as min/max, and to balance as left/right. For balance, `l`, `r`, and `c`, are aliases for `0.0`, `0.5`, and `1.0`, respectively.

Capitalize `V` or `B` to display the volume or balance.

Pass `--visual` with `V` and/or `B` to display volume and balance as ASCII sliders.

(See the man page/help for more options.)

I originally wrote this to easily correct the output balance after rebooting from Boot Camp:

```shell
$ sndctl -b c
```

----

© 2017-2019 Nate Weaver (Wevah)/Derailer
