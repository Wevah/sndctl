# sndctl

v1.3 (2022)
by Nate Weaver (Wevah)  
https://derailer.org/

A small utility for controlling sound output from the command line on macOS.

<pre class="shell">$ sndctl -v <var>volume</var>
$ sndctl -b <var>balance</var></pre>

Volume and balance are from `0.0` to `1.0`, translated to volume as min/max, and to balance as left/right.  
For balance, `l`, `r`, and `c`, are aliases for `0.0`, `1.0`, and `0.5`, respectively.

Capitalize `V` or `B` to display the volume or balance.

Pass `--visual` with `V` and/or `B` to display volume and balance as ASCII sliders.

(See the man page/help for more options.)

I originally wrote this to easily correct the output balance after rebooting:

<pre class="shell">$ sndctl -b c</pre>

Note: Getting/setting the balance on some outputs on Apple Silicon macs doesn't work.

----

© 2017-2022 Nate Weaver (Wevah)/Derailer
