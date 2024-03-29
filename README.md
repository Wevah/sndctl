# sndctl

v1.3.1 (2023)
by Nate Weaver (Wevah)  
https://derailer.org/

A small utility for controlling sound output from the command line on macOS.

<pre class="shell">$ sndctl -v <var>volume</var>
$ sndctl -b <var>balance</var></pre>

Volume and balance are from `0.0` to `1.0`, translated to volume as min/max, and to balance as left/right.  
For balance, `l`, `r`, and `c`, are aliases for `0.0`, `1.0`, and `0.5`, respectively.

Capitalize `V` or `B` to display the volume or balance.

Pass `--visual` with `V` and/or `B` to display volume and balance as ASCII sliders.

List output devices and supported properties with `--list`/`-l` (colored if `CLICOLOR` is set):

```console
$ sndctl -l
44: Display Audio
    has volume:  yes
    has balance: yes
65: External Headphones
    has volume:  yes
    has balance: no
53: MacBook Pro Speakers
    has volume:  yes
    has balance: no
```

(See the man page/help for more options.)

I originally wrote this to easily correct the output balance after rebooting:

<pre class="shell">$ sndctl -b c</pre>

Note: Getting/setting the balance on some outputs on Apple Silicon macs doesn't work.

----

© 2017-2023 Nate Weaver (Wevah)/Derailer
